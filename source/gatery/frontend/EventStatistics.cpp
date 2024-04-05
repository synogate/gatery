/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

	Gatery is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 3 of the License, or (at your option) any later version.

	Gatery is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "DesignScope.h"
#include "Clock.h"
#include "EventStatistics.h"
#include "SimSigHandle.h"
#include <gatery/hlim/NodeGroup.h>



namespace gtry {

	void EventStatistics::addEvent(std::string_view name, const Bit& trigger) {
		auto clk = ClockScope::getClk();
		auto pathName = getNodePath(name);
		m_counter[pathName] = 0;

		DesignScope::get()->getCircuit().addSimulationProcess([=]()->SimProcess {
			while (true)
			{
				co_await OnClk(clk);
				if (simu(trigger) == '1')
					m_counter[pathName] += 1;
			}
			});

	};

	void EventStatistics::protDumpStatistics() const {
		std::cout << "Signal statistics" << std::endl;
		for (const auto& elems : m_counter)
			std::cout << elems.first << "|" << elems.second << std::endl;
	};

	size_t EventStatistics::protReadEventCounter(std::string_view name) const {
		auto itCounter = m_counter.find(name);
		HCL_DESIGNCHECK_HINT(itCounter != m_counter.end(), "An event counter with this name was never registered");
		return itCounter->second;
	};

	void EventStatistics::protWriteStatTable(const std::filesystem::path &file_name) const {
		std::ofstream file;
		file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
		file.open(file_name.string(), std::fstream::out);
		file << "Signal name;Counter value;\n";
		for (const auto& elems : m_counter)
			file << elems.first << ";" << elems.second << ";\n";
		file.close();
		//std::cout << "Statistic table written to " << file_name << std::endl;
	};

	std::string EventStatistics::getNodePath(std::string_view name) const
	{
		auto currentEntity = GroupScope::getCurrentNodeGroup();
		std::string pathName = std::string(name);

		while (currentEntity != nullptr)
		{
			pathName = currentEntity->getInstanceName()+ "/" + pathName;
			currentEntity = currentEntity->getParent();
		}

		return pathName;
	};

	void registerEvent(std::string_view name, const Bit& trigger) {

		EventStatistics::get()->addEvent(name, trigger);

	}

	
}
