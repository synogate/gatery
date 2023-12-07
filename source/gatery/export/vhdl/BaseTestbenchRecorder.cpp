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

#include "BaseTestbenchRecorder.h"

#include "AST.h"
#include "Entity.h"

#include "../../simulation/Simulator.h"
#include "../../hlim/coreNodes/Node_Pin.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/supportNodes/Node_ExportOverride.h"
#include "../../hlim/RevisitCheck.h"

namespace gtry::vhdl {

BaseTestbenchRecorder::BaseTestbenchRecorder(AST *ast, sim::Simulator &simulator, std::string name) : m_ast(ast), m_simulator(simulator), m_name(std::move(name))
{
}

BaseTestbenchRecorder::~BaseTestbenchRecorder()
{
}

void BaseTestbenchRecorder::declareSignals(std::ostream &stream)
{
	auto *rootEntity = m_ast->getRootEntity();
	CodeFormatting &cf = m_ast->getCodeFormatting();

	for (auto clock : m_clocksOfInterest) {
		stream << "	SIGNAL " << rootEntity->getNamespaceScope().getClock(clock).name << " : STD_LOGIC := ";
		auto val = m_simulator.getValueOfClock(clock);
		if (!val[sim::DefaultConfig::DEFINED])
			stream << "'X';" << std::endl;
		else
			if (val[sim::DefaultConfig::VALUE])
				stream << "'1';" << std::endl;
			else
				stream << "'0';" << std::endl;		
	}
	for (auto clock : m_resetsOfInterest) {
		stream << "	SIGNAL " << rootEntity->getNamespaceScope().getReset(clock).name << " : STD_LOGIC := ";
		auto val = m_simulator.getValueOfReset(clock);
		if (!val[sim::DefaultConfig::DEFINED])
			stream << "'X';" << std::endl;
		else
			if (val[sim::DefaultConfig::VALUE])
				stream << "'1';" << std::endl;
			else
				stream << "'0';" << std::endl;
	}

	for (auto ioPin : m_allIOPins) {
		const auto &decl = rootEntity->getNamespaceScope().get(ioPin);

		stream << "	SIGNAL " << decl.name << " : ";
		cf.formatConnectionType(stream, decl);
		stream << ';' << std::endl;
	}

	stream << "	SIGNAL TB_testbench_is_done : STD_LOGIC := '0';\n";

}

void BaseTestbenchRecorder::writePortmap(std::ostream &stream)
{
	auto *rootEntity = m_ast->getRootEntity();
	CodeFormatting &cf = m_ast->getCodeFormatting();

	std::vector<std::string> portmapList;

	for (auto &s : m_clocksOfInterest) {
		std::stringstream line;
		line << rootEntity->getNamespaceScope().getClock(s).name << " => ";
		line << rootEntity->getNamespaceScope().getClock(s).name;
		portmapList.push_back(line.str());
	}
	for (auto &s : m_resetsOfInterest) {
		std::stringstream line;
		line << rootEntity->getNamespaceScope().getReset(s).name << " => ";
		line << rootEntity->getNamespaceScope().getReset(s).name;
		portmapList.push_back(line.str());
	}
	for (auto &s : m_allIOPins) {
		std::stringstream line;
		line << rootEntity->getNamespaceScope().get(s).name << " => ";
		line << rootEntity->getNamespaceScope().get(s).name;
		portmapList.push_back(line.str());
	}

	for (auto i : utils::Range(portmapList.size())) {
		cf.indent(stream, 2);
		stream << portmapList[i];
		if (i+1 < portmapList.size())
			stream << ",";
		stream << std::endl;
	}

}

void BaseTestbenchRecorder::buildClockProcess(std::ostream &stream, const hlim::Clock *clock)
{
	CodeFormatting &cf = m_ast->getCodeFormatting();
	auto *rootEntity = m_ast->getRootEntity();
	const std::string &clockName = rootEntity->getNamespaceScope().getClock(clock).name;

	cf.indent(stream, 1);
	stream << "clock_process_" << clockName << " : PROCESS" << std::endl;
	cf.indent(stream, 1);
	stream << "BEGIN" << std::endl;	

	auto halfPeriod = hlim::ClockRational(1,2) / clock->absoluteFrequency();

	cf.indent(stream, 2);
	stream << "WAIT FOR ";
	hlim::formatTime(stream, halfPeriod);
	stream << ';' << std::endl;

	cf.indent(stream, 2);
	stream << clockName << " <= not " << clockName << ";\n";

	cf.indent(stream, 2);
	stream << "IF TB_testbench_is_done = '1' THEN WAIT; END IF;" << std::endl;  

	cf.indent(stream, 1);
	stream << "END PROCESS;" << std::endl;

}

void BaseTestbenchRecorder::findClocksAndPorts()
{
	auto *rootEntity = m_ast->getRootEntity();

	const auto &allClocks = rootEntity->getClocks();
	const auto &allResets = rootEntity->getResets();
	const auto &allIOPins = rootEntity->getIoPins();

	for (auto &c : allClocks)
		if (c->isSelfDriven(false, true))
			m_clocksOfInterest.insert(c);

	for (auto &c : allResets)
		if (c->isSelfDriven(false, false))
			m_resetsOfInterest.insert(c);

	for (auto &p : allIOPins)
		if (!p->getPinNodeParameter().simulationOnlyPin)
			m_allIOPins.insert(p);

}

hlim::Node_Pin *BaseTestbenchRecorder::isDrivenByPin(hlim::NodePort nodePort) const
{
	if (nodePort.node == nullptr) return nullptr;

	hlim::RevisitCheck revisitCheck(*nodePort.node->getCircuit());

	while (nodePort.node != nullptr) {
		if (revisitCheck.contains(nodePort.node)) return nullptr;
		revisitCheck.insert(nodePort.node);

		if (auto *pin = dynamic_cast<hlim::Node_Pin*>(nodePort.node))
			return pin;
		else if (auto *signal = dynamic_cast<hlim::Node_Signal*>(nodePort.node))
			nodePort = signal->getDriver(0);
		else if (auto *overide = dynamic_cast<hlim::Node_ExportOverride*>(nodePort.node))
			nodePort = overide->getDriver(hlim::Node_ExportOverride::EXP_INPUT);
		else
			return nullptr;		
	}
	return nullptr;
}

}
