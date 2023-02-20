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

#include "CpuTrace.h"
#include "../Avalon.h"

namespace gtry::scl
{
	template<class... Capability> struct TileLinkU;
	using TileLinkUL = TileLinkU<>;
}

namespace gtry::scl::riscv
{
	struct Instruction
	{
		UInt opcode = 5_b; // without 11b lsb's
		UInt rd = 5_b;
		UInt rs1 = 5_b;
		UInt rs2 = 5_b;
		UInt func3 = 3_b;
		UInt func7 = 7_b;
		UInt immI = 32_b;
		UInt immS = 32_b;
		UInt immB = 32_b;
		UInt immU = 32_b;
		UInt immJ = 32_b;

		// used for debugging
		UInt instruction = 32_b;
		UInt name = 32_b;

		void decode(const UInt& inst);
	};

	struct IntAluResult
	{
		UInt sum = 32_b;
		Bit zero;
		Bit overflow;
		Bit sign;
		Bit carry;
	};

	struct IntAluCtrl
	{
		UInt op1 = 32_b;
		UInt op2 = 32_b;
		Bit sub;

		void result(IntAluResult& result) const;
	};

	class RV32I
	{
	public:
		RV32I(BitWidth instructionAddrWidth = 32_b, BitWidth dataAddrWidth = 32_b);
		void ipOffset(uint32_t offset) { m_IPoffset = offset; }

		virtual void execute();
		virtual void selectInstructions();

		// instruction implementations
		virtual void lui();
		virtual void auipc();
		virtual void jal();
		virtual void branch();
		virtual void arith();
		virtual void logic();
		virtual void setcmp();
		virtual void shift();

		virtual void csr(); // base implementation returns zero for all registers
		virtual void csrMachineInformation(uint32_t vendorId = 0, uint32_t architectureId = 0, uint32_t implementationId = 0, uint32_t hartId = 0, uint32_t configPtr = 0);
		virtual void csrCycle(BitWidth regW = 64_b);
		virtual void csrTime(BitWidth regW = 64_b, hlim::ClockRational resolution = { 1, 1'000'000 }); // default time resolution is 1us
		virtual void csrInstructionsRetired(BitWidth regW = 64_b);

		virtual void mem(AvalonMM& mem, bool byte = true, bool halfword = true);
		virtual TileLinkUL memTLink(bool byte = true, bool halfword = true);

		void setupAlu();

		const CpuTrace& trace() const { return m_trace; }
	protected:
		virtual void store(AvalonMM& mem, bool byte, bool halfword);
		virtual void load(AvalonMM& mem, bool byte, bool halfword);
		virtual void csrRegister(size_t address, const UInt& data);

	protected:
		// helper for instructions
		virtual void setIP(const UInt& ip) = 0;
		virtual void setResult(const UInt& result);
		virtual void setStall(const Bit& wait);

		Area m_area;

		uint32_t m_IPoffset = 0;
		UInt m_IP;
		UInt m_IPnext;
		Bit m_stall;
		Bit m_resultValid;
		UInt m_resultData = 32_b;

		Instruction m_instr;
		Bit m_discardResult;
		
		UInt m_r1 = 32_b;
		UInt m_r2 = 32_b;

		IntAluCtrl m_alu;
		IntAluResult m_aluResult;

		BitWidth m_dataAddrWidth;
		CpuTrace m_trace;

	};

	class SingleCycleI : public RV32I
	{
	public:

		SingleCycleI(BitWidth instructionAddrWidth = 32_b, BitWidth dataAddrWidth = 32_b);

		virtual Memory<UInt>& fetch(uint32_t firstInstructionAddr = 0);
		virtual UInt fetch(const UInt& instruction, uint32_t firstInstructionAddr = 0);
		virtual void fetchOperands(BitWidth regAddrWidth = 5_b);

	protected:
		virtual void setIP(const UInt& ip);

		UInt m_resultIP;
		Memory<UInt> m_rf;
		Memory<UInt> m_instructionMem;

	};


}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::riscv::Instruction, opcode, rd, rs1, rs2, func3, func7, immI, immS, immB, immU, immJ, name);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::riscv::IntAluResult, sum, zero, overflow, sign, carry);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::riscv::IntAluCtrl, op1, op2, sub);
