/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "GhdlSimulation.h"

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <boost/serialization/vector.hpp>

#include <algorithm>
#include <sstream>
#include <atomic>
#include <iostream>

static std::atomic<size_t> g_IdCounter = 0;

static std::string generateUniqueName()
{
	std::string ret = "mhdl_child_";
#ifndef _DEBUG
	ret += std::to_string(boost::this_process::get_id());
	ret += "_";
#endif
	ret += std::to_string(g_IdCounter++);
	return ret;
}

vpi_client::GhdlSimulation::GhdlSimulation() :
	m_InstanceName(generateUniqueName())
{
}

vpi_client::GhdlSimulation::~GhdlSimulation()
{
	if (m_GhdlProcess.running())
		m_GhdlProcess.terminate();

	if (m_CmdQueueP2C)
	{
		m_CmdQueueP2C.reset();
		ipc::message_queue::remove((m_InstanceName + "_P2C").c_str());
	}

	if (m_CmdQueueC2P)
	{
		m_CmdQueueC2P.reset();
		ipc::message_queue::remove((m_InstanceName + "_C2P").c_str());
	}
}

void vpi_client::GhdlSimulation::launch(std::string_view topEntity, const vpi_client::GenericsVector& generics)
{
	if (m_GhdlProcess.running())
		throw std::runtime_error{ "ghdl process still running" };

	auto ghdl_path = bp::search_path("ghdl");
	if (ghdl_path.empty())
		throw std::runtime_error{ "ghdl not found in PATH" };

	// update ghdl lib in case vhdl sources did change
	if (bp::system(ghdl_path, "-m", std::string{ topEntity }) != 0)
		throw std::runtime_error{ "ghdl make failed" };

	// find path of vpi lib
	auto vpi_host_path = boost::dll::program_location().parent_path() / 
		("vpi_simulation_host" + boost::dll::shared_library::suffix().string());
	if (!fs::is_regular_file(vpi_host_path))
		throw std::runtime_error{ "vpi host shared library not found. expected at: " + vpi_host_path.string() };

	// create list of generics
	std::vector<std::string> generic_params;
	std::transform(generics.begin(), generics.end(), back_inserter(generic_params), [](const auto& g) {
		std::ostringstream s;
		s << "-g" << g.first << '=' << g.second;
		return s.str();
	});

	bp::environment env = boost::this_process::environment();

	// create communication channel
	auto parent2child = m_InstanceName + "_p2c";
	env["HCL_VPI_CMDQUEUE_P2C"] = parent2child;
	ipc::message_queue::remove(parent2child.c_str());
	m_CmdQueueP2C.emplace(ipc::create_only, parent2child.c_str(), 10, 1024);

	auto child2parent = m_InstanceName + "_c2p";
	env["HCL_VPI_CMDQUEUE_C2P"] = child2parent;
	ipc::message_queue::remove(child2parent.c_str());
	m_CmdQueueC2P.emplace(ipc::create_only, child2parent.c_str(), 10, 1024);

#ifdef _WIN32
	// for some reason libghdlvpi is located in a seperate path. so we need to make sure LoadLibrary can find it.
	auto vpi_path = ghdl_path.parent_path().parent_path() / "lib";
	if (!fs::is_regular_file(vpi_path / "libghdlvpi.dll"))
		throw std::runtime_error{ "libghdlvpi.dll not found in ghdl/lib folder" };

	assert(!env["Path"].empty());
	env["Path"] += vpi_path.string();
#endif
	
	m_GhdlProcess = bp::child(ghdl_path, env, "-r", std::string{ topEntity },
		generic_params,
		"--vpi=" + vpi_host_path.string()
	);

	loadSimulationInfo();
}

int vpi_client::GhdlSimulation::exit()
{
	if (!m_GhdlProcess.running())
		throw std::runtime_error{ "ghdl instance not running" };

	m_CmdQueueP2C->send("e", 1, 0);

	if (!m_GhdlProcess.wait_for(std::chrono::seconds(2)))
	{
		std::cerr << "ghdl had to be terminated" << std::endl;
		m_GhdlProcess.terminate();
	}
	return m_GhdlProcess.exit_code();
}

template<typename T>
inline T vpi_client::GhdlSimulation::loadResponse()
{
	std::vector<uint8_t> buffer(m_CmdQueueC2P->get_max_msg_size());
	unsigned len, prio;
	m_CmdQueueC2P->receive(buffer.data(), buffer.size(), len, prio);
	buffer.resize(len);

	boost::iostreams::array_source source{ (char*)buffer.data(), buffer.size() };
	boost::iostreams::stream is{ source }; 
	boost::archive::binary_iarchive a{ is };

	T ret;
	a >> ret;
	return ret;
}

void vpi_client::GhdlSimulation::loadSimulationInfo()
{
	m_CmdQueueP2C->send("I", 1, 0);
	m_SimInfo = loadResponse<vpi_host::SimInfo>();
}

