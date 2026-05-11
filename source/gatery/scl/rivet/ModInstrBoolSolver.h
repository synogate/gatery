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
#include <external/minbool/minbool.h>

#include "ModInstrFetch.h"

namespace gtry::scl::rivet
{
	struct InstrOpcode;
	struct InstrDef;
}

namespace gtry::scl::rivet::mod
{
	enum class MatchValue { Low, High, DontCare };

	template<typename Base>
	class InstrBoolSolver : public Base
	{
		struct Rule
		{
			uint8_t mask;
			uint8_t value;
		};

	public:


	protected:

		virtual Bit buildInstrCondition(std::span<const minbool::MinTerm<8>> solution, const BVec& opcode) const
		{
			Bit ret = '0';
			for (const auto& term : solution)
			{
				Bit val = '1';
				for (size_t i = 0; i < 8; ++i)
					switch (size_t(term[i]))
					{
					case 0: val &= !opcode[i]; break;
					case 1: val &= opcode[i]; break;
					default:;
					}
				ret |= val;
			}
			return ret;
		}

		virtual std::optional<Bit> findInstrCondition(const InstrOpcode& instr, std::span<const MatchValue> actions, bool unknownIsDontCare = true) const
		{
			std::span<const Rule> rules = this->rules();
			HCL_ASSERT(actions.size() == Base::instructions().size());

			std::vector<uint8_t> on, dc;
			for (size_t i = 0; i < 256; ++i)
			{
				bool match = false;
				for (size_t r = 0; r < rules.size(); ++r)
					if (actions[r] != MatchValue::DontCare && (rules[r].mask & i) == rules[r].value)
					{
						match = true;
						if (actions[r] == MatchValue::High)
							on.push_back(uint8_t(i));
						break;
					}

				if (!match && unknownIsDontCare)
					dc.push_back(uint8_t(i));
			}

			std::vector<minbool::MinTerm<8>> solution = minbool::minimize_boolean<8>(on, dc);
			if (solution.empty())
				return {};

			return buildInstrCondition(solution, opcodeFromInstr(instr));
		}

		virtual std::optional<BVec> buildMux(std::span<const std::optional<Bit>> selector, std::span<const std::optional<BVec>> source)
		{
			std::optional<BVec> out;
			for(size_t i = 0; i < selector.size() && i < source.size(); ++i)
				if (selector[i] && source[i])
				{
					if (!out) out = *selector[i] & *source[i];
					else *out |= zext(*selector[i] & *source[i]);
				}
			return out;
		}

		template<typename T>
		std::optional<BVec> selectByInstr(const InstrOpcode& instr, std::span<const std::optional<BVec>> source, std::string namePrefix, std::function<T(const InstrDef&)> proj)
		{
			std::vector<std::optional<Bit>> selector(source.size());
			for (size_t i = 0; i < source.size(); ++i)
			{
				selector[i] = findInstrCondition(instr, instrAction({ T(i) }, { T::DontCare }, proj));
				if(selector[i])
					*selector[i] = *selector[i] | name(std::format("{}_{}", namePrefix, magic_enum::enum_name(T(i))));
			}
			return buildMux(selector, source);
		}

		template<typename Ttype, typename Tproj>
		std::vector<MatchValue> instrAction(std::initializer_list<Ttype> include, std::initializer_list<Ttype> dontCare, Tproj&& proj) const
		{
			std::vector<MatchValue> ret;
			ret.reserve(Base::instructions().size());

			for (const InstrDef& def : Base::instructions())
			{
				const Ttype val = std::invoke(proj, def);
				if (auto it = std::find(include.begin(), include.end(), val); it != include.end())
					ret.push_back(MatchValue::High);
				else if (auto it = std::find(dontCare.begin(), dontCare.end(), val); it != dontCare.end())
					ret.push_back(MatchValue::DontCare);
				else
					ret.push_back(MatchValue::Low);
			}

			return ret;
		}

		std::array<std::string_view, 256> instrNameMap() const
		{
			std::span rules = this->rules();
			std::array<std::string_view, 256> nameMap;
			for (size_t i = 0; i < nameMap.size(); ++i)
				for(size_t r = 0; r < rules.size(); ++r)
					if ((i & rules[r].mask) == rules[r].value)
					{
						nameMap[i] = Base::instructions()[r].name;
						break;
					}
			return nameMap;
		}

		static BVec opcodeFromInstr(const InstrOpcode& instr)
		{
			return cat(instr.instr(12, 3_b), instr.instr(2, 5_b));
		}
	private:


		std::span<const Rule> rules() const
		{
			if(m_rules.empty())
			{
				m_rules.reserve(Base::instructions().size());
				for (const InstrDef& def : Base::instructions())
				{
					Rule r{};

					for (uint8_t i = 0; i < 8; ++i)
					{
						char digit;
						if (i < 5) digit = def.opcode[4 - i];
						else digit = def.func3.empty() ? '-' : def.func3[2 - (i - 5)];

						if (digit != '-')
							r.mask |= 1 << i;
						if (digit == '1')
							r.value |= 1 << i;
					}

					m_rules.push_back(r);
				}

				for (size_t i = 0; i < 256; ++i)
				{
					const Rule* first = nullptr;
					for (const Rule& r : m_rules)
						if ((r.mask & i) == r.value)
						{
							if (first)
								throw std::runtime_error("overlapping instruction codes");
							first = &r;
						}
				}
			}

			HCL_ASSERT(m_rules.size() == Base::instructions().size());
			return m_rules;
		}

		mutable std::vector<Rule> m_rules;
	};
}
