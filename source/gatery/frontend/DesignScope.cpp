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

#include <gatery/export/DotExport.h>

namespace gtry {

	DesignScope::DesignScope() : DesignScope(std::make_unique<DefaultTargetTechnology>())
	{ 
	}

	DesignScope::DesignScope(std::unique_ptr<TargetTechnology> targetTech) : 
				BaseScope<DesignScope>(), 
				m_rootScope(m_circuit.getRootNodeGroup()), 
				m_targetTech(std::move(targetTech))
	{ 
		m_rootScope.setName("top");
		
		HCL_DESIGNCHECK_HINT(m_parentScope == nullptr, "Only one design scope can be active at a time!");
		m_defaultTechScope.emplace(m_targetTech->getTechCaps());
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
