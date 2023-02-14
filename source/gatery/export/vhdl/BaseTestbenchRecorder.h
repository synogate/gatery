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

#include "../../simulation/SimulatorCallbacks.h"

#include <string>
#include <vector>
#include <set>
#include <ostream>

namespace gtry::sim {
	class Simulator;
}


namespace gtry::hlim {
	class Node_Pin;
}

namespace gtry::vhdl {

class AST;

class BaseTestbenchRecorder : public sim::SimulatorCallbacks
{
	public:
		//BaseTestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, const std::string &name);
		BaseTestbenchRecorder(AST *ast, sim::Simulator &simulator, std::string name);
		virtual ~BaseTestbenchRecorder();

		inline const std::vector<std::string> &getDependencySortedEntities() const { return m_dependencySortedEntities; }
		inline const std::vector<std::string> &getAuxiliaryDataFiles() const { return m_auxiliaryDataFiles; }

		inline const std::string &getName() const { return m_name; }
	protected:
		AST *m_ast;
		sim::Simulator &m_simulator;
		std::string m_name;
		std::vector<std::string> m_dependencySortedEntities;
		std::vector<std::string> m_auxiliaryDataFiles;

		std::set<const hlim::Clock*> m_clocksOfInterest;
		std::set<const hlim::Clock*> m_resetsOfInterest;
		std::set<const hlim::Node_Pin*> m_allIOPins;

		std::map<hlim::NodePort, std::string> m_outputToIoPinName;

		struct Phase {
			std::stringstream assertStatements;
			std::map<std::string, std::string> signalOverrides;
			std::map<std::string, std::string> resetOverrides;
		};

		std::vector<Phase> m_phases;
		Phase m_postDuringPhase;

		void findClocksAndPorts();

		void declareSignals(std::ostream &stream);
		void writePortmap(std::ostream &stream);

		void buildClockProcess(std::ostream &stream, const hlim::Clock *clock);
};


}
