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

namespace gtry::sim
{


	VCDSink::VCDSink(hlim::Circuit& circuit, Simulator& simulator, const char* filename, const char* logFilename) :
		WaveformRecorder(circuit, simulator),
		m_VCD(filename)
	{
		if(logFilename != nullptr) {
			m_logFile.open(logFilename, std::fstream::binary);
			if(!m_logFile)
				throw std::runtime_error(std::string("Could not open log file for writing: ") + logFilename);
		}

		auto clockPins = hlim::extractClockPins(circuit, hlim::Subnet::allForSimulation(circuit));

		for(auto& clk : clockPins.clockPins)
			m_clocks.push_back(clk.source);

		for(auto& rst : clockPins.resetPins)
			m_resets.push_back(rst.source);
	}

	void VCDSink::onDebugMessage(const hlim::BaseNode* src, std::string msg)
	{}

	void VCDSink::onWarning(const hlim::BaseNode* src, std::string msg)
	{}

	void VCDSink::onAssert(const hlim::BaseNode* src, std::string msg)
	{}

	class VCDIdentifierGenerator {
	public:
		enum {
			IDENT_BEG = 33,
			IDENT_END = 127
		};
		VCDIdentifierGenerator()
		{
			m_nextIdentifier.reserve(10);
			m_nextIdentifier.resize(1);
			m_nextIdentifier[0] = IDENT_BEG;
		}
		std::string getIdentifer()
		{
			std::string res = m_nextIdentifier;

			unsigned idx = 0;
			while(true) {
				if(idx >= m_nextIdentifier.size()) {
					m_nextIdentifier.push_back(IDENT_BEG);
					break;
				}
				else {
					m_nextIdentifier[idx]++;
					if(m_nextIdentifier[idx] >= IDENT_END) {
						m_nextIdentifier[idx] = IDENT_BEG;
						idx++;
					}
					else break;
				}
			}

			return res;
		}
	protected:
		std::string m_nextIdentifier;
	};

	void VCDSink::initialize()
	{
		VCDIdentifierGenerator identifierGenerator;
		m_id2sigCode.resize(m_id2Signal.size());

		struct Module {
			std::map<const hlim::NodeGroup*, Module> subModules;
			std::vector<std::pair<hlim::NodePort, size_t>> signals;
		};

		Module root;

		for(auto id : utils::Range(m_id2Signal.size())) {
			auto& signal = m_id2Signal[id];
			m_id2sigCode[id] = identifierGenerator.getIdentifer();

			std::vector<const hlim::NodeGroup*> nodeGroupTrace;
			const hlim::NodeGroup* grp = signal.nodeGroup;
			while(grp != nullptr) {
				nodeGroupTrace.push_back(grp);
				grp = grp->getParent();
			}
			Module* m = &root;
			for(auto it = nodeGroupTrace.rbegin(); it != nodeGroupTrace.rend(); ++it)
				m = &m->subModules[*it];
			m->signals.push_back({ signal.driver, id });
		}

		std::function<void(const Module*)> reccurWriteModules;
		reccurWriteModules = [&](const Module* module) {
			for(const auto& p : module->subModules) {
				auto named_module = m_VCD.beginModule(p.first->getInstanceName());
				reccurWriteModules(&p.second);
			}

			for(const auto& sigId : module->signals) {
				if(m_id2Signal[sigId.second].isHidden)
					continue;

				auto width = hlim::getOutputWidth(sigId.first);
				m_VCD.declareWire(width, m_id2sigCode[sigId.second], m_id2Signal[sigId.second].name);
			}

			auto hidden_module = m_VCD.beginModule("__hidden");
			for(const auto& sigId : module->signals) {
				if(!m_id2Signal[sigId.second].isHidden)
					continue;

				auto width = hlim::getOutputWidth(sigId.first);
				m_VCD.declareWire(width, m_id2sigCode[sigId.second], m_id2Signal[sigId.second].name);
			}
		};

		reccurWriteModules(&root);

		{
			auto clocks_module = m_VCD.beginModule("clocks");

			for(auto& clk : m_clocks) {
				std::string id = identifierGenerator.getIdentifer();
				m_clock2code[clk] = id;
				m_VCD.declareWire(1, id, clk->getName());
			}

			for(auto& rst : m_resets) {
				std::string id = identifierGenerator.getIdentifer();
				m_rst2code[rst] = id;
				m_VCD.declareWire(1, id, rst->getResetName());
			}
		}

		auto dumpvars = m_VCD.beginDumpVars();

		for(auto& c : m_clock2code) {
			auto value = m_simulator.getValueOfClock(c.first);
			if(value[DefaultConfig::DEFINED])
				m_VCD.writeBitState(c.second, true, value[DefaultConfig::VALUE]);
		}

		for(auto& c : m_rst2code) {
			auto value = m_simulator.getValueOfReset(c.first);
			if(value[DefaultConfig::DEFINED])
				m_VCD.writeBitState(c.second, true, value[DefaultConfig::VALUE]);
		}
	}

	void VCDSink::signalChanged(size_t id)
	{
		const auto& offsetSize = m_id2StateOffsetSize[id];
		if(offsetSize.size == 1 && !m_id2Signal[id].isBVec)
			m_VCD.writeBitState(m_id2sigCode[id],
				m_trackedState.get(DefaultConfig::DEFINED, offsetSize.offset),
				m_trackedState.get(DefaultConfig::VALUE, offsetSize.offset)
			);
		else
			m_VCD.writeState(m_id2sigCode[id], m_trackedState, offsetSize.offset, offsetSize.size);
	}

	void VCDSink::advanceTick(const hlim::ClockRational& simulationTime)
	{
		auto ratTickIdx = simulationTime / hlim::ClockRational(1, 1'000'000'000'000ull);
		size_t tickIdx = ratTickIdx.numerator() / ratTickIdx.denominator();
		m_VCD.writeTime(tickIdx);
	}

	void VCDSink::onClock(const hlim::Clock* clock, bool risingEdge)
	{
		auto it = m_clock2code.find((hlim::Clock*)clock);
		if(it != m_clock2code.end())
			m_VCD.writeBitState(it->second, true, risingEdge);
	}

	void VCDSink::onReset(const hlim::Clock* clock, bool inReset)
	{
		auto it = m_rst2code.find((hlim::Clock*)clock);
		if(it != m_rst2code.end())
			m_VCD.writeBitState(it->second, true, inReset);
	}

}
