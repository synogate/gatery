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
#include "../../hlim/Node.h"
#include "../../hlim/supportNodes/Node_Memory.h"
#include "../Simulator.h"
#include "../../hlim/postprocessing/ClockPinAllocation.h"
#include "../../hlim/postprocessing/CDCDetection.h"
#include "../../hlim/Subnet.h"


#include <boost/format.hpp>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <string>
#include <functional>
#include <filesystem>

namespace gtry::sim
{
	namespace {
		const char *syntheticModuleName = "synthetic";
		const char *debugMessagesLabel = "Debug_Messages";
		const char *warningsLabel = "Warnings";
		const char *assertsLabel = "Asserts";
	}

	VCDSink::VCDSink(hlim::Circuit& circuit, Simulator& simulator, const char* filename, const char* logFilename) :
		WaveformRecorder(circuit, simulator),
		m_VCD(filename)
	{
		if(logFilename != nullptr) {
			auto parentPath = std::filesystem::path(logFilename).parent_path();
			if (!parentPath.empty())
				std::filesystem::create_directories(parentPath);		
			m_logFile.open(logFilename, std::fstream::binary);
			if(!m_logFile)
				throw std::runtime_error(std::string("Could not open log file for writing: ") + logFilename);
		}

		m_gtkWaveProjectFile.setWaveformFile(filename);

		auto clockPins = hlim::extractClockPins(circuit, hlim::Subnet::allForSimulation(circuit));

		for(auto& clk : clockPins.clockPins)
			m_clocks.push_back(clk.source);

		for(auto& rst : clockPins.resetPins)
			m_resets.push_back(rst.source);
	}
	VCDSink::~VCDSink() 
	{
		writeGtkWaveProjFile();
	}

	void VCDSink::writeGtkWaveProjFile()
	{
		m_gtkWaveProjectFile.write((m_gtkWaveProjectFile.getWaveformFile()+".gtkw").c_str());
		m_gtkWaveProjectFile.writeSurferScript(m_gtkWaveProjectFile.getWaveformFile() + ".surfer");
	}

	void VCDSink::onDebugMessage(const hlim::BaseNode* src, std::string msg)
	{
		if (m_includeDebugMessages)
			m_VCD.writeString(m_debugMessageID, msg);
	}

	void VCDSink::onWarning(const hlim::BaseNode* src, std::string msg)
	{
		if (m_includeWarnings)
			m_VCD.writeString(m_warningsID, msg);
	}

	void VCDSink::onAssert(const hlim::BaseNode* src, std::string msg)
	{
		if (m_includeAsserts)
			m_VCD.writeString(m_assertsID, msg);
		try {
			m_gtkWaveProjectFile.addMarker(m_simulator.getCurrentSimulationTime());
		} catch (...) {
		}
	}

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
			utils::StableMap<const hlim::NodeGroup*, Module> subModules;
			std::vector<std::pair<hlim::NodePort, size_t>> signals;
			utils::StableMap<hlim::Node_Memory*, std::vector<size_t>> memoryWords;
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

			if (signal.signalRef.driver.node != nullptr)
				m->signals.push_back({ signal.signalRef.driver, id });
			else
				m->memoryWords[signal.memory].push_back(id);
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

			for(const auto& mem : module->memoryWords) {
				auto named_module = m_VCD.beginModule("memory_"+mem.first->getName());
				for(const auto& sigId : mem.second) {
					auto& signal = m_id2Signal[sigId];
					m_VCD.declareWire(signal.memoryWordSize, m_id2sigCode[sigId], m_id2Signal[sigId].name);
				}
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

		if (m_includeDebugMessages || m_includeWarnings || m_includeAsserts) {
			auto synthetic_module = m_VCD.beginModule(syntheticModuleName);
			if (m_includeDebugMessages) {
				m_debugMessageID = identifierGenerator.getIdentifer();
				m_VCD.declareString(m_debugMessageID, debugMessagesLabel);
			}
			if (m_includeWarnings) {
				m_warningsID = identifierGenerator.getIdentifer();
				m_VCD.declareString(m_warningsID, warningsLabel);
			}
			if (m_includeAsserts) {
				m_assertsID = identifierGenerator.getIdentifer();
				m_VCD.declareString(m_assertsID, assertsLabel);
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

		setupGtkWaveProjFileSignals();
		m_gtkWaveProjectFile.writeEnumFilterFiles();
		writeGtkWaveProjFile();
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

	void VCDSink::onCommitState()
	{
		WaveformRecorder::onCommitState();

		if (m_commitCounter++ % 128 == 0)
			m_VCD.commit();
	}

	void VCDSink::setupGtkWaveProjFileSignals()
	{
		if (m_includeDebugMessages)
			m_gtkWaveProjectFile.appendSignal(std::string(syntheticModuleName)+'.'+debugMessagesLabel).color = GTKWaveProjectFile::Signal::GREEN;
		if (m_includeWarnings)
			m_gtkWaveProjectFile.appendSignal(std::string(syntheticModuleName)+'.'+warningsLabel).color = GTKWaveProjectFile::Signal::ORANGE;
		if (m_includeAsserts)
			m_gtkWaveProjectFile.appendSignal(std::string(syntheticModuleName)+'.'+assertsLabel).color = GTKWaveProjectFile::Signal::RED;

		if (m_includeDebugMessages || m_includeWarnings || m_includeAsserts)
		m_gtkWaveProjectFile.appendBlank();

		// For easier extension in the future (beyond IO pins) determine clock domains for all signals so
		// that they can be sorted by clocks without relying on the clock ports of the io pins.
		utils::UnstableMap<hlim::NodePort, hlim::SignalClockDomain> clockDomains;
		hlim::inferClockDomains(m_circuit, clockDomains);

		utils::StableMap<hlim::Clock*, std::vector<Signal*>> signalsByClocks;

		for (auto &s : m_id2Signal) {
			if (!s.isPin && !s.isTap) continue;

			auto it = clockDomains.find(s.signalRef.driver);
			if (it == clockDomains.end() || it->second.type != hlim::SignalClockDomain::CLOCK)
				signalsByClocks[nullptr].push_back(&s);
			else
				signalsByClocks[it->second.clk].push_back(&s);
		}


		auto constructFullSignalName = [](const Signal &signal) {
			std::string name = signal.name;
			auto *grp = signal.nodeGroup;
			while (grp != nullptr) {
				name = grp->getInstanceName() + '.' + name;
				grp = grp->getParent();
			}
			return name;
		};

		for (auto &clockDomain : signalsByClocks) {
			if (clockDomain.first != nullptr) {
				auto *clockPin = clockDomain.first->getClockPinSource();
				m_gtkWaveProjectFile.appendSignal(std::string("clocks.")+clockPin->getName()).color = GTKWaveProjectFile::Signal::BLUE;
				if(auto* rstPin = clockDomain.first->getResetPinSource())
					m_gtkWaveProjectFile.appendSignal(std::string("clocks.")+rstPin->getResetName()).color = GTKWaveProjectFile::Signal::INDIGO;
			}

			m_gtkWaveProjectFile.appendBlank();

			std::sort(clockDomain.second.begin(), clockDomain.second.end(), [](Signal* lhs, Signal* rhs)->bool{
				return lhs->sortOrder < rhs->sortOrder;
			});

			for (auto *signal : clockDomain.second) {
				std::string vcdName = constructFullSignalName(*signal);

				auto connectionType = hlim::getOutputConnectionType(signal->signalRef.driver);

				// GTKWave does not include 1 bit vectors in the signal list, so we need to treat them as bits
				if (!connectionType.isBool() && connectionType.width > 1)
					vcdName = (boost::format("%s[%d:0]") % vcdName % (connectionType.width-1)).str();

				m_gtkWaveProjectFile.appendSignal(vcdName);
			}

			m_gtkWaveProjectFile.appendBlank();
		}
	}

}
