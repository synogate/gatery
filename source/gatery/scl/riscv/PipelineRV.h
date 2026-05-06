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
#include "riscv.h"
#include "../stream/strm.h"
#include "../rivet/ModRoot.h"
#include "../rivet/ModControlFlow.h"
#include "../rivet/ModInstrSet.h"
#include "../rivet/ModInstrBoolSolver.h"
#include "../rivet/ModInstrDecode.h"
#include "../rivet/ModResultSelect.h"
#include "../rivet/ModLoadStore.h"
#include "../rivet/ModIntAlu.h"
#include "../rivet/ModBaseIntInstrSet.h"
#include "../rivet/ModFenceNoop.h"
#include "../rivet/ModShiftUnit.h"
#include "../rivet/ModEnvCallBreak.h"
#include "../rivet/ModBranch.h"
#include "../rivet/ModInstrRom.h"
#include "../rivet/ModDataMemory.h"
#include "../rivet/ModBranchMissPredict.h"
#include "../rivet/ModCsr.h"
#include "../rivet/ModHazardData.h"

#include "../rivet/ModGateMate.h"

#include "../io/uart.h"

#include <external/minbool/minbool.h>

namespace gtry::scl::rivet
{
	class PipelineRV : public
	//	mod::GateMate<
		mod::InstrRom<
		mod::DataMemory<
		mod::Csr<
		mod::HazardDataResolve<
		mod::BranchMissPredict<
		mod::BaseIntInstrSet<
		mod::Branch<
		mod::FenceNoop<
		mod::LoadStore<
		mod::EnvCallBreak<
		mod::ShiftUnit<
		mod::ControlFlow<
		mod::RegisterFile<
		mod::IntAlu<
		mod::ResultSelect<
		mod::InstrDecode<
		mod::InstrBoolSolver<
		mod::InstrSet<
		mod::Root>>>>>>>>>>>>>>>>>>
	{
	public:
		PipelineRV()
		{
			environmentCallAddress("32b0");

			const unsigned char gcd_bin[] = {
				0x13, 0x00, 0x00, 0x00,   0x13, 0x01, 0x00, 0x40,   0x93, 0x00, 0x00, 0x00,
				0x93, 0x04, 0x40, 0x00,   0x13, 0x09, 0x80, 0x00,   0x83, 0xa0, 0x00, 0x00,
				0x83, 0xa4, 0x04, 0x00,   0x63, 0x8c, 0x90, 0x00,   0x63, 0xd6, 0x14, 0x00,
				0xb3, 0x80, 0x90, 0x40,   0x6f, 0xf0, 0x5f, 0xff,   0xb3, 0x84, 0x14, 0x40,
				0x6f, 0xf0, 0xdf, 0xfe,   0x23, 0x20, 0x19, 0x00,   0x6f, 0xf0, 0x1f, 0xfd,
				0x13, 0x01, 0x01, 0xfe,   0x23, 0x2e, 0x81, 0x00,   0x13, 0x04, 0x01, 0x02,
				0x23, 0x26, 0xa4, 0xfe,   0x23, 0x24, 0xb4, 0xfe,   0x03, 0x27, 0xc4, 0xfe,
				0x83, 0x27, 0x84, 0xfe,   0x63, 0x0c, 0xf7, 0x02,   0x03, 0x27, 0xc4, 0xfe,
				0x83, 0x27, 0x84, 0xfe,   0x63, 0xdc, 0xe7, 0x00,   0x03, 0x27, 0xc4, 0xfe,
				0x83, 0x27, 0x84, 0xfe,   0xb3, 0x07, 0xf7, 0x40,   0x23, 0x26, 0xf4, 0xfe,
				0x6f, 0xf0, 0x9f, 0xfd,   0x03, 0x27, 0x84, 0xfe,   0x83, 0x27, 0xc4, 0xfe,
				0xb3, 0x07, 0xf7, 0x40,   0x23, 0x24, 0xf4, 0xfe,   0x6f, 0xf0, 0x5f, 0xfc,
				0x83, 0x27, 0xc4, 0xfe,   0x13, 0x85, 0x07, 0x00,   0x03, 0x24, 0xc1, 0x01,
				0x13, 0x01, 0x01, 0x02,   0x67, 0x80, 0x00, 0x00
			};
			instrRom(std::as_bytes(std::span(gcd_bin)));


			const unsigned gcd_parameter[] = {
				210, 126,
			};
			dataMemContent(std::as_bytes(std::span(gcd_parameter)));
			dataMemSize(2048);


			addCsrCycle(64_b);
			addCsrTime(32_b, 1ms);
			addCsrInstructionsRetired(32_b);
		}

		auto stageInstrPtr() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						attach(addInstrPtr()) |
						attach(addInstrSize());
				};
		}

		auto stageDecode() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						attach(addInstrName()) | // move to instr name class
						strm::then(checkForIllegalInstructionCode()) | // move to seperate class
						attach(decodeImmediate()) |
						attach(addCsrCtrl()) |
						attach(addRegCtrl()) |
						attach(addJmpCtrl()) |
						attach(addMemCtrl());
				};
		}

		auto stageOperands() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						attach(readRegisterFile()) |
						attach(selectCsrReadData()) |
						attach(selectOperands<OperandsSum>("sum", &InstrDef::sumSrc)) |
						attach(selectOperands<OperandsDiff>("diff", &InstrDef::difSrc));
				};
		}

		auto stageExecute() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						strm::attach(ResultCollection{}) |
						attach(aluDiff()) |
						attach(aluSum()) |
						attach(selectAluResults()) |
						attach(selectMemCtrl()) |
						attach(ecallUnit()) |
						attach(shiftUnit());
				};
		}

		auto stageResults() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						attach(selectRegWriteValue()) |
						attach(selectJmpTarget());
				};
		}

		void generate()
		{
			VStream<BVec> result;

			Stream ret =
				strm::createVStream(Nothing{}, '1') |
				strm::attach(Ready{}) |

				stageInstrPtr() |
				instrFetch() |
				pipelineStage("fetch") |
				stageDecode() |
				stageOperands() |
				pipelineStage("decode") |
				attach(hazardDataForwardingSum()) |
				attach(hazardDataForwardingDiff()) |
				stageExecute() |
				dataMem() |
				strm::then([&](const MemCtrl& mem) { result = strm::createVStream(mem.memWriteData.lower(8_b), mem.memWrite & EnableScope::getFullEnable()); }) |
				stageResults() |
				attach(makeBranchDecision()) |
				pipelineStage("execute") |

				strm::removeReady('1') |
				name("result");
			tap(ret);
			resultLoopBack(move(ret));

			Bit txPin = uartTx(move(result) | strm::regDownstream() | strm::attach(Ready{}) | strm::fifo(512), 230400);
			pinOut(txPin, "UART0_TX");


		}
	};
}
