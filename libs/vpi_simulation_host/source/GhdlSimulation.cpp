#include "GhdlSimulation.h"

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>

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

	if (m_CmdQueue)
	{
		m_CmdQueue.reset();
		ipc::message_queue::remove(m_InstanceName.c_str());
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
	ipc::message_queue::remove(m_InstanceName.c_str());
	m_CmdQueue.emplace(ipc::create_only, m_InstanceName.c_str(), 10, 1024);
	env["MHDL_VPI_CMDQUEUE"] = m_InstanceName;

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
}

int vpi_client::GhdlSimulation::exit()
{
	if (!m_CmdQueue || !m_GhdlProcess.running())
		throw std::runtime_error{ "ghdl instance not running" };

	m_CmdQueue->send("e", 1, 0);

	if (!m_GhdlProcess.wait_for(std::chrono::seconds(2)))
	{
		std::cerr << "ghdl had to be terminated" << std::endl;
		m_GhdlProcess.terminate();
	}
	return m_GhdlProcess.exit_code();
}

