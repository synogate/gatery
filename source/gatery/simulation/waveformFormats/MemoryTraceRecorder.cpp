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
#include "MemoryTraceRecorder.h"

#include "../../hlim/Circuit.h"
#include "../../hlim/Clock.h"

namespace gtry::sim {

MemoryTraceRecorder::MemoryTraceRecorder(MemoryTrace &trace, hlim::Circuit &circuit, Simulator &simulator, bool startImmediately) : WaveformRecorder(circuit, simulator), m_trace(trace)
{
	m_trace.clear();
}


void MemoryTraceRecorder::start()
{
	m_record = true;
	HCL_ASSERT_HINT(false, "Not implemented yet!");
}

void MemoryTraceRecorder::stop()
{
	m_record = false;
	HCL_ASSERT_HINT(false, "Not implemented yet!");
}


void MemoryTraceRecorder::onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc)
{
	auto &an = m_trace.annotations[id];
	an.ranges.push_back({.desc = desc, .start = simulationTime });
}

void MemoryTraceRecorder::onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id)
{
	auto it = m_trace.annotations.find(id);
	HCL_DESIGNCHECK_HINT(it != m_trace.annotations.end(), "Ending an annotation that never started!");
	HCL_DESIGNCHECK_HINT(!it->second.ranges.empty(), "Ending an annotation that never started!");
	it->second.ranges.back().end = simulationTime;
}



void MemoryTraceRecorder::onDebugMessage(const hlim::BaseNode *src, std::string msg)
{
}

void MemoryTraceRecorder::onWarning(const hlim::BaseNode *src, std::string msg)
{
}

void MemoryTraceRecorder::onAssert(const hlim::BaseNode *src, std::string msg)
{
}

void MemoryTraceRecorder::onClock(const hlim::Clock *clock, bool risingEdge)
{
	auto it = m_clock2idx.find(clock);
	HCL_ASSERT(it != m_clock2idx.end());
	MemoryTrace::SignalChange sigChange;
	sigChange.sigIdx = it->second;
	sigChange.dataOffset = m_bitAllocator.allocate(1);
	m_trace.events.back().changes.push_back(sigChange);
	m_trace.data.resize(m_bitAllocator.getTotalSize());
	m_trace.data.set(sim::DefaultConfig::DEFINED, sigChange.dataOffset, true);
	m_trace.data.set(sim::DefaultConfig::VALUE, sigChange.dataOffset, risingEdge);
}

void MemoryTraceRecorder::initialize()
{
	for (auto &sig : m_id2Signal) {
		m_trace.signals.push_back({
			.driver = sig.signalRef.driver,
			.name = sig.name,
			.width = hlim::getOutputWidth(sig.signalRef.driver),
			.isBool = !sig.isBVec,
		});
	}


	for (auto &clk : m_circuit.getClocks()) {
		m_clock2idx[clk.get()] = m_trace.signals.size();
		m_trace.signals.push_back({
			.clock = clk.get(),
			.name = clk->getName(),
			.width = 1,
			.isBool = true
		});
	}
	m_trace.events.push_back({
		.timestamp = hlim::ClockRational(0,1)
	});

}

void MemoryTraceRecorder::signalChanged(size_t id)
{
	MemoryTrace::SignalChange sigChange;
	sigChange.sigIdx = id;
	sigChange.dataOffset = m_bitAllocator.allocate(m_trace.signals[id].width);
	m_trace.events.back().changes.push_back(sigChange);
	m_trace.data.resize(m_bitAllocator.getTotalSize());
	m_trace.data.copyRange(sigChange.dataOffset, m_trackedState, m_id2StateOffsetSize[id].offset, m_id2StateOffsetSize[id].size);
}

void MemoryTraceRecorder::advanceTick(const hlim::ClockRational &simulationTime)
{
	m_trace.events.push_back({
		.timestamp = simulationTime
	});
}


}
