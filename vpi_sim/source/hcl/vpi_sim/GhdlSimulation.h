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
#pragma once
#include "vpi_simulation_host.h"

#include <string_view>
#include <vector>
#include <optional>
#include <boost/process.hpp>
#include <boost/interprocess/ipc/message_queue.hpp>

namespace vpi_client
{
	namespace bp = boost::process;
	namespace fs = boost::filesystem;
	namespace ipc = boost::interprocess;

	using GenericsVector = std::vector<std::pair<std::string_view, std::string_view>>;

	class GhdlSimulation
	{
	public:
		GhdlSimulation();
		~GhdlSimulation();

		void launch(std::string_view topEntity, const GenericsVector& generics);
		int exit();

		const vpi_host::SimInfo& info() const { return m_SimInfo; }

	protected:
		void loadSimulationInfo();

		template<typename T>
		T loadResponse();

	private:
		const std::string m_InstanceName;
		bp::child m_GhdlProcess;
		std::optional<ipc::message_queue> m_CmdQueueP2C;
		std::optional<ipc::message_queue> m_CmdQueueC2P;

		vpi_host::SimInfo m_SimInfo;
		
	};


}
