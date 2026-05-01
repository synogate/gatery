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
#include "../stream/strm.h"

#include "ModInstrFetch.h"
#include "ModRegisterFile.h"
#include "ModCsr.h"

namespace gtry::scl::rivet
{
	struct InstrDef;

	struct Imm
	{
		BVec imm;
	};

	struct InstrName
	{
		BVec instrName;
	};
}

namespace gtry::scl::rivet::mod
{
	template<typename Base>
	class InstrDecode : public Base
	{
	public:


	protected:

		auto decodeImmediate() {
			return [this](const InstrOpcode& instr)
				{
					auto solve = [&](std::initializer_list<InstrEncoding> active) {
						return Base::findInstrCondition(instr, Base::instrAction(active, { InstrEncoding::DontCare, InstrEncoding::R }, &InstrDef::encoding)).value_or('0');
					};

					Bit ifS = solve({ InstrEncoding::S }) | name("ifS");
					Bit ifI = solve({ InstrEncoding::I }) | name("ifI");
					Bit ifB = solve({ InstrEncoding::SB }) | name("ifB");
					Bit ifU = solve({ InstrEncoding::U }) | name("ifU");
					Bit ifJ = solve({ InstrEncoding::UJ }) | name("ifJ");
					Bit ifIJ = solve({ InstrEncoding::I, InstrEncoding::UJ }) | name("ifIJ");
					Bit ifUJ = solve({ InstrEncoding::U, InstrEncoding::UJ }) | name("ifUJ");
					Bit ifSB = solve({ InstrEncoding::S, InstrEncoding::SB }) | name("ifSB");
					Bit ifIS = solve({ InstrEncoding::I, InstrEncoding::S }) | name("ifIS");
					Bit ifISB = solve({ InstrEncoding::I, InstrEncoding::S, InstrEncoding::SB }) | name("ifISB");
					Bit ifISBJ = solve({ InstrEncoding::I, InstrEncoding::S, InstrEncoding::SB, InstrEncoding::UJ }) | name("ifISBJ");

					const BVec& i = instr.instr;
					BVec ret = cat(
						i[31],
						(i(20, 11_b) & ifU) | (i[31] & ifISBJ),
						(i(12, 8_b) & ifUJ) | (i[31] & ifISB),
						(i[31] & ifIS) | (i[7] & ifB) | (i[20] & ifJ),
						(i(25, 6_b) & ifISBJ),
						(i(21, 4_b) & ifIJ) | (i(8, 4_b) & ifSB),
						(i[20] & ifI) | (i[7] & ifS)
					);

					return Imm{ ret };
				};
		}

		template<typename Tout>
		auto selectOperands(std::string_view sigName, std::function<const std::array<OperandSource, 2>&(const InstrDef&)> proj)
		{
			return [=, this](const InstrOpcode& instr, const Imm& imm, const RegOperands& reg, const InstrPtr& ip, const InstrSize& size, const CsrCtrl& csr)
				{
					std::array<std::optional<BVec>, (size_t)OperandSource::DontCare> source;
					source[(size_t)OperandSource::IP] = zext(ip.instrPtr, Base::dataPathW());
					source[(size_t)OperandSource::Imm] = imm.imm;
					source[(size_t)OperandSource::Zero] = ConstBVec(0, Base::dataPathW());
					source[(size_t)OperandSource::InvInstrSize] = invInstrSize(size);
					source[(size_t)OperandSource::CSR] = csr.csrReadData;

					BVec out[2];
					for (size_t i = 0; i < 2; ++i)
					{
						source[(size_t)OperandSource::Reg] = reg.regOperand[i];
						auto elemProj = [i, proj](const InstrDef& def) { return proj(def)[i]; };
						out[i] = Base::template selectByInstr<OperandSource>(
							instr, source, std::format("select_{}_{}", sigName, i), elemProj
						).value_or(ConstBVec(Base::dataPathW()));
					}
					return Tout{ out[0], out[1] };
				};
		}

		auto checkForIllegalInstructionCode()
		{
			return [this](const InstrOpcode& instr) 
				{
					std::vector<MatchValue> action(Base::instructions().size(), MatchValue::High);

					Bit validInstruction = Base::findInstrCondition(instr, action, false).value_or('1') | name("instruction_code_valid");
					sim_assert(validInstruction) << "illegal instr";
				};
		}

		auto addInstrName() { return [this](const InstrOpcode& instr)
			{
				std::array<std::string_view, 256> nameMap = Base::instrNameMap();

				BitWidth nameW = 0_b;
				for (const InstrDef& def : Base::instructions())
					nameW = std::max(nameW, BitWidth{ def.name.size() * 8 });

				std::vector<std::byte> nameRomData;
				nameRomData.reserve(nameW.bytes() * nameMap.size());
				for (std::string_view name : nameMap)
					for (size_t i = 0; i < nameW.bytes(); ++i)
						if (nameW.bytes() - 1 - i < name.size())
							nameRomData.push_back((std::byte)name[nameW.bytes() - 1 - i]);
						else
							nameRomData.push_back({});

				Memory<BVec> nameLookupRom(nameMap.size(), nameW);
				nameLookupRom.setName("instrNameRom");
				nameLookupRom.setType(MemType::DONT_CARE, 0);
				nameLookupRom.fillPowerOnState(sim::createDefaultBitVectorState(nameRomData));
				
				UInt opcode = (UInt)Base::opcodeFromInstr(instr);
				BVec name = simOverride(ConstBVec(nameW), nameLookupRom[opcode].read());
				return InstrName{ name };
			};
		}


	private:
		BVec invInstrSize(const InstrSize& size) const
		{
			BVec ret = ConstBVec((Base::dataPathW()).mask() & ~0b11, Base::dataPathW());
			// for compressed instr the size is 2
			ret[1] = size.instrLogSize.lsb();

			return ret;
		}


	};
}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::Imm, imm);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::rivet::InstrName, instrName);
