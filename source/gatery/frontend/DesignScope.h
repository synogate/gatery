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
#pragma once

#include "Comments.h"
#include <gatery/hlim/Circuit.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/simulation/ConstructionTimeSimulationContext.h>

#include "Scope.h"
#include "tech/TargetTechnology.h"

#include <memory>
#include "EventStatistics.h"

namespace gtry {

class Clock;
class ClockScope;

/**
 * @addtogroup gtry_scopes
 * @{
 */

/**
 * @brief The design scope holds the circuit and provides a context for all circuti building operations.
 * 
 */
class DesignScope : public BaseScope<DesignScope>
{
	public:
		DesignScope(std::string_view topName = "top");
		DesignScope(std::unique_ptr<TargetTechnology> targetTech, std::string_view topName = "top");

		/// Specifies a target technology (such as a specific FPGA device) to target with this design.
		void setTargetTechnology(std::unique_ptr<TargetTechnology> targetTech);

		/// Visualizes the circuit (or node group) by creating a .dot file for graphviz and attempting to run graphviz on it.
		static void visualize(const std::string &filename, hlim::NodeGroup *nodeGroup = nullptr);

		/// Visualizes the circuit around a node by creating a .dot file for graphviz and attempting to run graphviz on it.
		static void visualizeAround(const std::string &filename, size_t nodeId, size_t dilation);

		static DesignScope *get() { return m_currentScope; }
		hlim::Circuit &getCircuit() { return m_circuit; }
		const hlim::Circuit &getCircuit() const { return m_circuit; }
		
		template<typename NodeType, typename... Args>
		static NodeType *createNode(Args&&... args);		
		
		template<typename ClockType, typename... Args>
		static ClockType *createClock(Args&&... args);

		inline GroupScope &getRootGroup() { return m_rootScope; }
		utils::PropertyTree instanceProperties(bool settingsOnly = false) const;

		template<typename Type = TargetTechnology>
		inline Type *getTargetTechnology() { return dynamic_cast<Type*>(m_targetTech.get()); }

		/// Runs postprocessing (including technology mapping) in the created design.
		void postprocess();
	protected:
		hlim::Circuit m_circuit;
		GroupScope m_rootScope;
		
		std::unique_ptr<Clock> m_defaultClock;
		std::unique_ptr<ClockScope> m_defaultClockScope;

		sim::ConstructionTimeSimulationContext m_simContext;
		
		std::unique_ptr<TargetTechnology> m_targetTech;
		std::optional<TechnologyScope> m_defaultTechScope;

		EventStatistics m_eventStatistics;
};

template<typename NodeType, typename... Args>
NodeType *DesignScope::createNode(Args&&... args) {
	HCL_ASSERT(GroupScope::getCurrentNodeGroup() != nullptr);
	
	NodeType *node = m_currentScope->m_circuit.createNode<NodeType>(std::forward<Args>(args)...);
	node->recordStackTrace();
	node->moveToGroup(GroupScope::getCurrentNodeGroup());
	node->setComment(Comments::retrieve());
	return node;
}

template<typename ClockType, typename... Args>
ClockType *DesignScope::createClock(Args&&... args) {
	ClockType *node = m_currentScope->m_circuit.createClock<ClockType>(std::forward<Args>(args)...);
	return node;
}


template<class SimVizClass>
void addSimViz(const SimVizClass &simViz) 
{
	DesignScope::get()->getCircuit().addSimulationVisualization(simViz);
}
	


/** @}*/

}
