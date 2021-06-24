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
#include <gatery/frontend.h>

#include "../Avalon.h"

namespace gtry::scl::riscv
{
	struct Instruction
	{
		BVec opcode = 5_b; // without 11b lsb's
		BVec rd = 5_b;
		BVec rs1 = 5_b;
		BVec rs2 = 5_b;
		BVec func3 = 3_b;
		BVec func7 = 7_b;
		BVec immI = 32_b;
		BVec immS = 32_b;
		BVec immB = 32_b;
		BVec immU = 32_b;
		BVec immJ = 32_b;

		BVec name = 32_b;

		void decode(const BVec& inst);
	};

	struct IntAluResult
	{
		BVec sum = 32_b;
		Bit zero;
		Bit overflow;
		Bit sign;
		Bit carry;
	};

	struct IntAluCtrl
	{
		BVec op1 = 32_b;
		BVec op2 = 32_b;
		Bit sub;

		void result(IntAluResult& result) const;
	};

	class RV32I
	{
	public:
		RV32I(BitWidth instructionAddrWidth = 32_b, BitWidth dataAddrWidth = 32_b);

		virtual void execute();

		// instruction implementations
		virtual void lui();
		virtual void auipc();
		virtual void jal();
		virtual void branch();
		virtual void arith();
		virtual void logic();
		virtual void setcmp();
		virtual void shift();
		virtual void mem(AvalonMM& mem, bool byte = true, bool halfword = true);
		virtual void store(AvalonMM& mem, bool byte, bool halfword);
		virtual void load(AvalonMM& mem, bool byte, bool halfword);

		void setupAlu();

	protected:
		// helper for instructions
		virtual void setIP(const BVec& ip) = 0;
		virtual void setResult(const BVec& result) = 0;
		virtual void setStall(const Bit& wait) = 0;

		BVec m_IP;
		BVec m_IPnext;

		Instruction m_instr;
		Bit m_instructionValid;
		
		BVec m_r1 = 32_b;
		BVec m_r2 = 32_b;

		IntAluCtrl m_alu;
		IntAluResult m_aluResult;

		BitWidth m_dataAddrWidth;

		Area m_area;

	};

	class SingleCycleI : public RV32I
	{
	public:

		SingleCycleI(BitWidth instructionAddrWidth = 32_b, BitWidth dataAddrWidth = 32_b);

		virtual Memory<BVec>& fetch();
		virtual BVec fetch(const BVec& instruction);
		virtual void fetchOperands(BitWidth regAddrWidth = 5_b);

	protected:
		virtual void setIP(const BVec& ip);
		virtual void setResult(const BVec& result);
		virtual void setStall(const Bit& wait);

		Bit m_stall;
		Bit m_resultValid;
		BVec m_resultData = 32_b;
		BVec m_resultIP;
		Memory<BVec> m_rf;
		Memory<BVec> m_instructionMem;

	};


}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::riscv::Instruction, opcode, rd, rs1, rs2, func3, func7, immI, immS, immB, immU, immJ, name);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::riscv::IntAluResult, sum, zero, overflow, sign, carry);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::riscv::IntAluCtrl, op1, op2, sub);
