#include "DesignScope.h"
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
#include "../hlim/Node.h"
#include "../hlim/NodeGroup.h"

#include "Clock.h"


#include <gatery/hlim/Subnet.h>
#include <gatery/export/DotExport.h>

namespace gtry {

	DesignScope::DesignScope(std::string_view topName) : DesignScope(std::make_unique<DefaultTargetTechnology>(), topName)
	{ 
	}

	DesignScope::DesignScope(std::unique_ptr<TargetTechnology> targetTech, std::string_view topName) :
				BaseScope<DesignScope>(), 
				m_circuit(topName),
				m_rootScope(m_circuit.getRootNodeGroup()), 
				m_targetTech(std::move(targetTech))
	{ 
		
		HCL_DESIGNCHECK_HINT(m_parentScope == nullptr, "Only one design scope can be active at a time!");
		m_defaultTechScope.emplace(m_targetTech->getTechCaps());

		m_defaultClock = std::make_unique<Clock>(ClockConfig{
				.absoluteFrequency = 1'000,
				.name = "GateryDefaultClock",
				.resetType = ClockConfig::ResetType::NONE, // this is also necessary to ensure sim_asserts work in eval-only contexts
				.memoryResetType = ClockConfig::ResetType::NONE,
		});
		m_defaultClockScope = std::make_unique<ClockScope>(*m_defaultClock);
	}

	void DesignScope::setTargetTechnology(std::unique_ptr<TargetTechnology> targetTech)
	{
		HCL_DESIGNCHECK_HINT(m_circuit.getNodes().empty(), "The target technology must be set before constructing the circuit!");
		m_targetTech = std::move(targetTech);
		m_defaultTechScope.emplace(m_targetTech->getTechCaps());
	}

	void DesignScope::visualize(const std::string &filename, hlim::NodeGroup *nodeGroup)
	{
		DotExport exp(filename+".dot");
		exp(get()->getCircuit(), nodeGroup);
		exp.runGraphViz(filename+".svg");
	}

	void DesignScope::visualizeAround(const std::string &filename, size_t nodeId, size_t dilation)
	{
		hlim::ConstSubnet subnet;

		for (const auto &node : get()->getCircuit().getNodes())
			if (node->getId() == nodeId) {
				subnet.add(node.get());
				break;
			}

		for ([[maybe_unused]] auto i : utils::Range(dilation))
			subnet.dilate(true, true);

		DotExport exp(filename+".dot");
		exp(get()->getCircuit(), subnet);
		exp.runGraphViz(filename+".svg");
	}


	utils::PropertyTree DesignScope::instanceProperties(bool settingsOnly) const
	{
		utils::PropertyTree ptree;
	
		std::function<void(const hlim::NodeGroup&)> fill_ptree = [&](const hlim::NodeGroup& group) -> void 
		{
			const utils::PropertyTree& source = settingsOnly ?
				group.usedSettings() :
				group.properties();

			if(!source.empty())
				ptree[group.instancePath()] = source;

			for (auto& child : group.getChildren())
				fill_ptree(*child);
		};
		fill_ptree(*m_rootScope.nodeGroup());

		return ptree;
	}

	void DesignScope::postprocess() 
	{
		m_circuit.postprocess(hlim::DefaultPostprocessing{m_targetTech->getTechnologyMapping()});
	}
}
