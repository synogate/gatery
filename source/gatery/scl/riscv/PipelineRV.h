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
#include "../rivet/ModExternalResult.h"
#include "../rivet/ModInstrSet.h"
#include "../rivet/ModInstrBoolSolver.h"
#include "../rivet/ModInstrDecode.h"
#include "../rivet/ModResultSelect.h"
#include "../rivet/ModLoadStore.h"
#include "../rivet/ModLoadStoreSubWord.h"
#include "../rivet/ModIntAlu.h"
#include "../rivet/ModBaseIntInstrSet.h"
#include "../rivet/ModFenceNoop.h"
#include "../rivet/ModShiftUnit.h"
#include "../rivet/ModShiftUnitSequential.h"
#include "../rivet/ModEnvCallBreak.h"
#include "../rivet/ModBranch.h"
#include "../rivet/ModInstrRom.h"
#include "../rivet/ModInstrTileLink.h"
#include "../rivet/ModDataMemory.h"
#include "../rivet/ModDataTileLink.h"
#include "../rivet/ModDataTileLinkReschedule.h"
#include "../rivet/ModBranchMissPredict.h"
#include "../rivet/ModCsr.h"
#include "../rivet/ModHazardData.h"

#include "../rivet/ModGateMate.h"

#include "../io/uart.h"

namespace gtry::scl::rivet
{
	class PipelineRV : public
	//	mod::GateMate<
	//	mod::InstrRom<
		mod::InstrTileLink<
	//	mod::DataMemory<
		mod::DataTileLinkReschedule<
		mod::ExternalResult<
		mod::Csr<
		mod::HazardDataResolve<
		mod::BranchMissPredict<
		mod::BaseIntInstrSet<
		mod::Branch<
		mod::FenceNoop<
	//	mod::LoadStoreSubword<
		mod::LoadStore<
		mod::EnvCallBreak<
	//	mod::ShiftUnit<
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
						strm::then(checkForIllegalInstructionCode()) | // move to separate class
						attach(decodeImmediate()) |
						attach(addCsrCtrl()) |
						attach(addRegCtrl()) |
						attach(addJmpCtrl()) |
						attach(addMemCtrl()) |
						attach(ecallDecode()) |
						attach(branchDecode()) |
						attach(aluDecode());
				};
		}

		auto stageOperands() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						attach(readRegisterFile()) |
						attach(selectCsrReadData()) |
						attach(selectOperands<SumOperands>("sum", &InstrDef::sumSrc)) |
						attach(selectOperands<AluOperands>("diff", &InstrDef::aluSrc));
				};
		}

		auto stageExecute() {
			return [this](StreamSignal auto&& in)
				{
					return move(in) |
						strm::attach(ResultCollection{}) |
						attach(aluExecute()) |
						attach(aluSum()) |
						attach(selectAluResults()) |
						attach(addExternalResult()) |
						attach(selectMemCtrl());// |
					//	shiftUnit();
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
			Stream ret =
				strm::createVStream(Nothing{}, '1') |
				strm::attach(Ready{}) |

				stageInstrPtr() |
				instrFetch() |
				stageDecode() |
				stageOperands() |
				pipelineStage("decode") |
				attach(hazardDataForwardingSum()) |
				attach(hazardDataForwardingDiff()) |
				stageExecute() |
				dataMem() |
				attach(shiftMemReadData()) |
				stageResults() |
				attach(branchDecision()) |
				pipelineStage("execute") |

				strm::removeReady('1') |
				name("result");
			//tap(ret);
			resultLoopBack(move(ret));
		}
	};
}
