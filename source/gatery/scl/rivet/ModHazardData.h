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
#include "ModRegisterFile.h"


namespace gtry::scl::rivet
{
	struct HazardDataForward
	{
		std::array<Bit, 2> regUsed;
		std::array<Bit, 2> regForward;
	};

	struct HazardData
	{
		std::array<Bit, 2> hazardDataNext;
		std::array<Bit, 2> hazardDataAny;

		HazardDataForward hazardDataForwardSum;
		HazardDataForward hazardDataForwardDiff;
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class HazardDataResolve : public Base
	{
		struct PerStageData
		{
			RegCtrl ctrl;
			HazardData hazard;
		};

	protected:

		template<typename Tattr, typename Tproj>
		auto hazardDataForwarding(Tproj&& proj)
		{
			return [this, proj](const Tattr& in, const HazardData& hazard)
				{
					auto [op] = in;
					std::array<BVec, 2> out;
					for (size_t i = 0; i < out.size(); ++i)
						out[i] = mux(std::invoke(proj, hazard).regForward[i], { op[i], m_regWriteData });
					return Tattr{ out };
				};
		}

		auto hazardDataForwardingSum() { return hazardDataForwarding<SumOperands>([](const HazardData& h) { return h.hazardDataForwardSum; }); }
		auto hazardDataForwardingDiff() { return hazardDataForwarding<AluOperands>([](const HazardData& h) { return h.hazardDataForwardDiff; }); }

		void resultLoopBack(StreamSignal auto&& result)
		{
			m_regWriteData = get<RegCtrl>(result).regWriteValue;
			Base::resultLoopBack(move(result));
		}

		auto pipelineStage(std::string_view name)
		{
			if(!m_regWriteData.valid())
				m_regWriteData = Base::dataPathW();

			return [this, name](StreamSignal auto&& in)
				{
					if constexpr (std::remove_cvref_t<decltype(in)>::template has<RegCtrl>())
					{
						const RegCtrl& wr = get<RegCtrl>(in);
						for (PerStageData& stage : m_prevStages)
							for (size_t i = 0; i < 2; ++i)
								stage.hazard.hazardDataAny[i] |= 
									valid(in) & 
									wr.regWrite & stage.ctrl.regRead[i] &
									wr.regWriteAddr == stage.ctrl.regReadAddr[i]
								;
						if (!m_prevStages.empty())
						{
							HazardData& hd = m_prevStages.back().hazard;
							hd.hazardDataNext = hd.hazardDataAny;

							// forward data in next cycle iff register was used as source and conflict has beed detected
							for (HazardDataForward* f : { &hd.hazardDataForwardSum, &hd.hazardDataForwardDiff })
								for (size_t i = 0; i < 2; ++i)
									f->regForward[i] = f->regUsed[i] & hd.hazardDataNext[i];
						}
					}

					if constexpr (std::remove_cvref_t<decltype(in)>::template has<RegOperands>())
					{
						m_prevStages.push_back({ get<RegCtrl>(in) });
						HazardData& hd = m_prevStages.back().hazard;
						
						Stream ret = move(in) | 
							strm::attach(hd) | 
							Base::pipelineStage(name);
						
						hd = prepareHazardData(get<InstrOpcode>(in));
						return ret;
					}
					else
					{
						return move(in) | 
							Base::pipelineStage(name);
					}
				};
		}

	private:
		HazardData prepareHazardData(const InstrOpcode& instr) const
		{
			HazardData hd = allZeros(HazardData{});
			auto findCondition = [&](auto&& proj) {
				return Base::findInstrCondition(
					instr,
					Base::instrAction({ OperandSource::Reg }, { OperandSource::DontCare }, move(proj))
				).value_or('0');
			};

			for (size_t i = 0; i < 2; ++i)
			{
				hd.hazardDataForwardSum.regUsed[i] = findCondition([i](const InstrDef& def) { return def.sumSrc[i]; });
				hd.hazardDataForwardDiff.regUsed[i] = findCondition([i](const InstrDef& def) { return def.aluSrc[i]; });
			}
			return hd;
		}

	private:
		std::list<PerStageData> m_prevStages;
		BVec m_regWriteData;
	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::HazardDataForward, regUsed, regForward);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::HazardData, hazardDataNext, hazardDataAny, hazardDataForwardSum, hazardDataForwardDiff);
