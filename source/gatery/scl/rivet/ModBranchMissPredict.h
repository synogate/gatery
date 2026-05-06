/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2026 Michael Offel, Andreas Ley

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
#include <gatery/frontend.h>
#include "ModInstrBoolSolver.h"
#include "ModControlFlow.h"
#include "ModIntAlu.h"

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class BranchMissPredict : public Base
	{
		struct PerStageSignals
		{
			Bit missPredicted;
			Bit onTransfer;
			Bit valid;
			Bit hadBranchMissPredict;
		};
	protected:

		auto pipelineStage(std::string_view name)
		{
			return [this, name](StreamSignal auto&& in) {
				return move(in) |
					Base::pipelineStage(name) |
					Base::attach(invalidateOnBranchTaken(name));
				};
		}

		template<StreamSignal T>
		void resultLoopBack(T&& result)
		{
			const JmpCtrl& ctrl = get<JmpCtrl>(result);
			m_missPredicted = (valid(result) & ctrl.jmp) | name("branch_miss_predicted");

			if (m_lastStage) // we dont want to invalidate the loop back stream
				m_lastStage->missPredicted = '0';

			return Base::resultLoopBack(move(result));
		}

	private:
		auto invalidateOnBranchTaken(std::string_view stageName)
		{
			std::shared_ptr stage = std::make_shared<PerStageSignals>();
			stage->hadBranchMissPredict = 
				flagInstantSet(stage->missPredicted, stage->onTransfer | !stage->valid) | 
				name(std::format("BranchMissPredict_{}_hadBranchMissPredict", stageName));

			return [this, stage](const Valid& in)
				{
					stage->onTransfer = EnableScope::getFullEnable();
					stage->valid = in.valid;
					Valid ret{
						in.valid & !stage->hadBranchMissPredict
					};
					if (m_lastStage)
						m_lastStage->missPredicted = m_missPredicted;
					m_lastStage = stage;
					return ret;
				};
		}

	private:
		Bit m_missPredicted;
		std::shared_ptr<PerStageSignals> m_lastStage;

	};
}
