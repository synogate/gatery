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

#include "BaseTestbenchRecorder.h"

#include <filesystem>

#include <fstream>
#include <map>
#include <set>
#include <sstream>

namespace gtry::sim {
	class Simulator;
}

namespace gtry::vhdl {

class VHDLExport;
class AST;

class TestbenchRecorder : public BaseTestbenchRecorder
{
	public:
		TestbenchRecorder(VHDLExport &exporter, AST *ast, sim::Simulator &simulator, std::filesystem::path basePath, std::string name);
		~TestbenchRecorder();

		virtual void onPowerOn() override;
		virtual void onAfterPowerOn() override;
		virtual void onNewTick(const hlim::ClockRational &simulationTime) override;
		virtual void onNewPhase(size_t phase) override;
		virtual void onAfterMicroTick(size_t microTick) override;
		virtual void onCommitState() override;
		virtual void onReset(const hlim::Clock *clock, bool resetAsserted) override;
		/*
		virtual void onDebugMessage(const hlim::BaseNode *src, std::string msg) override;
		virtual void onWarning(const hlim::BaseNode *src, std::string msg) override;
		virtual void onAssert(const hlim::BaseNode *src, std::string msg) override;
		*/
		virtual void onSimProcOutputOverridden(const hlim::NodePort &output, const sim::DefaultBitVectorState &state) override;
		virtual void onSimProcOutputRead(const hlim::NodePort &output, const sim::DefaultBitVectorState &state) override;


		virtual void onAnnotationStart(const hlim::ClockRational &simulationTime, const std::string &id, const std::string &desc) override;
		virtual void onAnnotationEnd(const hlim::ClockRational &simulationTime, const std::string &id) override;

	protected:
		VHDLExport &m_exporter;
		std::fstream m_testbenchFile;
		hlim::ClockRational m_writtenSimulationTime;
		hlim::ClockRational m_flushIntervalStart;

		void writeHeader();
		void writeFooter();

		void advanceTimeTo(const hlim::ClockRational &simulationTime);

		// Flushes all actions and tests to file by spreading the accumulated phases out between flushIntervalEnd the last flushIntervalEnd to allow simulator progression (and result inspection) between phases.
		void flush(const hlim::ClockRational &flushIntervalEnd);
};


}
