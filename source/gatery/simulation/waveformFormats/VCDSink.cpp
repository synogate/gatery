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
#include "VCDSink.h"

#include "../../hlim/NodeGroup.h"
#include "../../hlim/Circuit.h"
#include "../Simulator.h"
#include "../../hlim/postprocessing/ClockPinAllocation.h"
#include "../../hlim/Subnet.h"


#include <boost/format.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>
#include <functional>

namespace gtry::sim {


VCDSink::VCDSink(hlim::Circuit &circuit, Simulator &simulator, const char *filename, const char *logFilename) : WaveformRecorder(circuit, simulator)
{
	m_vcdFile.open(filename, std::fstream::binary);
	if (!m_vcdFile) 
		throw std::runtime_error(std::string("Could not open vcd file for writing!") + filename);

	if (logFilename != nullptr) {
		m_logFile.open(logFilename, std::fstream::binary);
		if (!m_logFile)
			throw std::runtime_error(std::string("Could not open log file for writing: ") + logFilename);
	}

	auto clockPins = hlim::extractClockPins(circuit, hlim::Subnet::allForSimulation(circuit));

	for (auto &clk : clockPins.clockPins)
		m_clocks.push_back(clk.source);

	for (auto &rst : clockPins.resetPins)
		m_resets.push_back(rst.source);
}

void VCDSink::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
}

void VCDSink::onWarning(const hlim::BaseNode *src, std::string msg)
{
}

void VCDSink::onAssert(const hlim::BaseNode *src, std::string msg)
{
}

class VCDIdentifierGenerator {
	public:
		enum {
			IDENT_BEG = 33,
			IDENT_END = 127
		};
		VCDIdentifierGenerator() {
			m_nextIdentifier.reserve(10);
			m_nextIdentifier.resize(1);
			m_nextIdentifier[0] = IDENT_BEG;
		}
		std::string getIdentifer() {
			std::string res = m_nextIdentifier;

			unsigned idx = 0;
			while (true) {
				if (idx >= m_nextIdentifier.size()) {
					m_nextIdentifier.push_back(IDENT_BEG);
					break;
				} else {
					m_nextIdentifier[idx]++;
					if (m_nextIdentifier[idx] >= IDENT_END) {
						m_nextIdentifier[idx] = IDENT_BEG;
						idx++;
					} else break;
				}
			}

			return res;
		}
	protected:
		std::string m_nextIdentifier;
};

void VCDSink::initialize()
{
	auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

	tm now_tb;
#ifdef WIN32
	localtime_s(&now_tb, &now);
#else
	localtime_r(&now, &now_tb);
#endif

	m_vcdFile
		<< "$date\n" << std::put_time(&now_tb, "%Y-%m-%d %X") << "\n$end\n"
		<< "$version\nGatery simulation output\n$end\n"
		<< "$timescale\n1ps\n$end\n";


	VCDIdentifierGenerator identifierGenerator;
	m_id2sigCode.resize(m_id2Signal.size());




	struct Module {
		std::map<const hlim::NodeGroup *, Module> subModules;
		std::vector<std::pair<hlim::NodePort, size_t>> signals;
	};

	Module root;

	for (auto id : utils::Range(m_id2Signal.size())) {
		auto &signal = m_id2Signal[id];
		m_id2sigCode[id] = identifierGenerator.getIdentifer();

		std::vector<const hlim::NodeGroup*> nodeGroupTrace;
		const hlim::NodeGroup *grp = signal.nodeGroup;
		while (grp != nullptr) {
			nodeGroupTrace.push_back(grp);
			grp = grp->getParent();
		}
		Module *m = &root;
		for (auto it = nodeGroupTrace.rbegin(); it != nodeGroupTrace.rend(); ++it)
			m = &m->subModules[*it];
		m->signals.push_back({signal.driver, id});
	}

	std::function<void(const Module*)> reccurWriteModules;
	reccurWriteModules = [&](const Module *module){
		for (const auto &p : module->subModules) {
			HCL_ASSERT(!p.first->getInstanceName().empty()); // gtkwave has some weird glitches if a module name is empty, so better fail here than wonder what happened.
			m_vcdFile
				<< "$scope module " << p.first->getInstanceName() << " $end\n";
			reccurWriteModules(&p.second);
			m_vcdFile
				<< "$upscope $end\n";
		}
		for (const auto &sigId : module->signals) {
			if (m_id2Signal[sigId.second].isHidden) continue;
			auto width = hlim::getOutputWidth(sigId.first);
			m_vcdFile
				<< "$var wire " << width << " " << m_id2sigCode[sigId.second] << " " << m_id2Signal[sigId.second].name << " $end\n";
		}
		m_vcdFile
			<< "$scope module __hidden $end\n";
		for (const auto &sigId : module->signals) {
			if (!m_id2Signal[sigId.second].isHidden) continue;
			auto width = hlim::getOutputWidth(sigId.first);
			m_vcdFile
				<< "$var wire " << width << " " << m_id2sigCode[sigId.second] << " " << m_id2Signal[sigId.second].name << " $end\n";
		}
		m_vcdFile
			<< "$upscope $end\n";
	};

	reccurWriteModules(&root);


	m_vcdFile
		<< "$scope module clocks $end\n";

	for (auto &clk : m_clocks) {
		std::string id = identifierGenerator.getIdentifer();
		m_clock2code[clk] = id;
		m_vcdFile
			<< "$var wire 1 " << id << " " << clk->getName() << " $end\n";
	}

	for (auto &rst : m_resets) {
		std::string id = identifierGenerator.getIdentifer();
		m_rst2code[rst] = id;
		m_vcdFile
			<< "$var wire 1 " << id << " " << rst->getResetName() << " $end\n";
	}

	m_vcdFile
		<< "$upscope $end\n";


	m_vcdFile
		<< "$enddefinitions $end\n"
		<< "$dumpvars\n";


	for (auto &c : m_clock2code) {
		auto value = m_simulator.getValueOfClock(c.first);
		if (value[DefaultConfig::DEFINED]) {
			if (value[DefaultConfig::VALUE])
				m_vcdFile << '1';
			else
				m_vcdFile << '0';
			m_vcdFile << c.second << "\n";
		}
	}

	for (auto &c : m_rst2code) {
		auto value = m_simulator.getValueOfReset(c.first);
		if (value[DefaultConfig::DEFINED]) {
			if (value[DefaultConfig::VALUE])
				m_vcdFile << '1';
			else
				m_vcdFile << '0';
			m_vcdFile << c.second << "\n";
		}
	}
}

void VCDSink::signalChanged(size_t id)
{
	const auto &offsetSize = m_id2StateOffsetSize[id];
	if (offsetSize.size == 1 && !m_id2Signal[id].isBVec) {
		stateToFile(offsetSize.offset, 1);
		m_vcdFile << m_id2sigCode[id] << "\n";
	} else {
		m_vcdFile << "b";
		stateToFile(offsetSize.offset, offsetSize.size);
		m_vcdFile << " " << m_id2sigCode[id] << "\n";
	}
}

void VCDSink::advanceTick(const hlim::ClockRational &simulationTime)
{
	auto ratTickIdx = simulationTime / hlim::ClockRational(1, 1'000'000'000'000ull);
	size_t tickIdx = ratTickIdx.numerator() / ratTickIdx.denominator();
	m_vcdFile << "#"<<tickIdx<<std::endl;
}


void VCDSink::stateToFile(size_t offset, size_t size)
{
	for (auto i : utils::Range(size)) {
		auto bitIdx = size-1-i;
		bool def = m_trackedState.get(DefaultConfig::DEFINED, offset + bitIdx);
		bool val = m_trackedState.get(DefaultConfig::VALUE, offset + bitIdx);
		if (!def)
			m_vcdFile << 'X';
		else
			if (val)
				m_vcdFile << '1';
			else
				m_vcdFile << '0';
	}
}

void VCDSink::onClock(const hlim::Clock *clock, bool risingEdge)
{
	auto it = m_clock2code.find((hlim::Clock *)clock);
	if (it != m_clock2code.end()) {
		if (risingEdge)
			m_vcdFile << '1';
		else
			m_vcdFile << '0';

		m_vcdFile << it->second << "\n";
	}
}


void VCDSink::onReset(const hlim::Clock *clock, bool inReset)
{
	auto it = m_rst2code.find((hlim::Clock *)clock);
	if (it != m_rst2code.end()) {
		if (inReset)
			m_vcdFile << '1';
		else
			m_vcdFile << '0';

		m_vcdFile << it->second << "\n";
	}
}

}
