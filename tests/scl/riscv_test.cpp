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
#include "scl/pch.h"
#include <boost/test/unit_test.hpp>
#include <boost/test/data/dataset.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>

#include <gatery/scl/riscv/riscv.h>
#include <gatery/scl/riscv/DualCycleRV.h>
#include <gatery/scl/algorithm/GCD.h>
#include <gatery/scl/riscv/ElfLoader.h>
#include <gatery/scl/riscv/EmbeddedSystemBuilder.h>
#include <gatery/scl/riscv/rvcc.h>
#include <gatery/scl/io/uart.h>
#include <gatery/scl/tilelink/tilelink.h>
#include <gatery/scl/tilelink/TileLinkHub.h>
#include <gatery/scl/tilelink/TileLinkValidator.h>


#include <queue>

using namespace boost::unit_test;
using namespace gtry;

namespace rv
{
	enum class op
	{
		LUI = 0x37,
		AUIPC = 0x17,
		JAL = 0x6F,
		JALR = 0x67,
		BRANCH = 0x63,
		LOAD = 0x03,
		STORE = 0x23,
		ARITHI = 0x13,
		ARITH = 0x33,
		FENCE = 0x0F,
		SYSTEM = 0x73
	};

	enum class func
	{
		// arith
		ADD = 0,
		SLL = 1,
		SLT	= 2,
		SLTU = 3,
		XOR = 4,
		SRL = 5,
		OR = 6,
		AND = 7,

		// branch
		BEQ = 0,
		BNE = 1,
		BLT = 4,
		BGE = 5,
		BLTU = 6,
		BGEU = 7,

		// load/store
		BYTE = 0,
		HALFWORD = 1,
		WORD = 2,
		BYTEU = 4,
		HALFWORDU = 5,

		// csr
		ECALL = 0,
		CSRRW = 1,
		CSRRS = 2,
		CSRRC = 3,
		CSRRWI = 5,
		CSRRSI = 6,
		CSRRCI = 7,
	};

	template<typename T, T mask>
	struct Imm
	{
		Imm(T val) : value(val& mask) {}
		operator T () { return value; }
		T value;
	};

	using ImmU = Imm<uint32_t, 0xFFFFF000ull>;
	using ImmJ = Imm<int32_t, 0x1FFFFE>;
	using ImmB = Imm<int32_t, 0x1FFE>;
	using ImmS = Imm<int32_t, 0xFFF>;
	using ImmI = Imm<int32_t, 0xFFF>;
}



struct RVOP
{
	RVOP& instruction(uint32_t icode) { simu(m_instructionWord) = icode; return *this; }

	void lui(size_t rd, rv::ImmU imm) { typeU(rv::op::LUI, rd, imm); }
	void auipc(size_t rd, rv::ImmU imm) { typeU(rv::op::AUIPC, rd, imm); }
	void jal(size_t rd, rv::ImmJ imm) { typeJ(rv::op::JAL, rd, imm); }
	void jalr(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::JALR, rv::func(0), rd, rs1, imm); }
	void beq(size_t rs1, size_t rs2, rv::ImmB imm) { typeB(rv::op::BRANCH, rv::func::BEQ, imm, rs1, rs2); }
	void bne(size_t rs1, size_t rs2, rv::ImmB imm) { typeB(rv::op::BRANCH, rv::func::BNE, imm, rs1, rs2); }
	void blt(size_t rs1, size_t rs2, rv::ImmB imm) { typeB(rv::op::BRANCH, rv::func::BLT, imm, rs1, rs2); }
	void bge(size_t rs1, size_t rs2, rv::ImmB imm) { typeB(rv::op::BRANCH, rv::func::BGE, imm, rs1, rs2); }
	void bltu(size_t rs1, size_t rs2, rv::ImmB imm) { typeB(rv::op::BRANCH, rv::func::BLTU, imm, rs1, rs2); }
	void bgeu(size_t rs1, size_t rs2, rv::ImmB imm) { typeB(rv::op::BRANCH, rv::func::BGEU, imm, rs1, rs2); }
	void lb(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::LOAD, rv::func::BYTE, rd, rs1, imm); }
	void lbu(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::LOAD, rv::func::BYTEU, rd, rs1, imm); }
	void lh(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::LOAD, rv::func::HALFWORD, rd, rs1, imm); }
	void lhu(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::LOAD, rv::func::HALFWORDU, rd, rs1, imm); }
	void lw(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::LOAD, rv::func::WORD, rd, rs1, imm); }
	void sb(size_t rs1, size_t rs2, rv::ImmS imm) { typeS(rv::op::STORE, rv::func::BYTE, imm, rs1, rs2); }
	void sh(size_t rs1, size_t rs2, rv::ImmS imm) { typeS(rv::op::STORE, rv::func::HALFWORD, imm, rs1, rs2); }
	void sw(size_t rs1, size_t rs2, rv::ImmS imm) { typeS(rv::op::STORE, rv::func::WORD, imm, rs1, rs2); }
	void addi(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::ADD, rd, rs1, imm); }
	void slti(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::SLT, rd, rs1, imm); }
	void sltui(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::SLTU, rd, rs1, imm); }
	void xori(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::XOR, rd, rs1, imm); }
	void ori(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::OR, rd, rs1, imm); }
	void andi(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::AND, rd, rs1, imm); }
	void slli(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::SLL, rd, rs1, imm); }
	void srli(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::SRL, rd, rs1, imm); }
	void srai(size_t rd, size_t rs1, rv::ImmI imm) { typeI(rv::op::ARITHI, rv::func::SRL, rd, rs1, imm.value | 1024); }
	void add(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::ADD, rd, rs1, rs2); }
	void sub(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::ADD, rd, rs1, rs2, 32); }
	void slt(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::SLT, rd, rs1, rs2); }
	void sltu(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::SLTU, rd, rs1, rs2); }
	void xor_(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::XOR, rd, rs1, rs2); }
	void or_(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::OR, rd, rs1, rs2); }
	void and_(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::AND, rd, rs1, rs2); }
	void sll(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::SLL, rd, rs1, rs2); }
	void srl(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::SRL, rd, rs1, rs2); }
	void sra(size_t rd, size_t rs1, size_t rs2) { typeR(rv::op::ARITH, rv::func::SRL, rd, rs1, rs2, 32); }

	RVOP& typeR(rv::op opcode, rv::func func3, size_t rd = 0, size_t rs1 = 0, size_t rs2 = 0, size_t func7 = 0)
	{
		uint32_t icode = (uint32_t)opcode;
		icode |= uint32_t(rd) << 7;
		icode |= uint32_t(func3) << 12;
		icode |= uint32_t(rs1) << 15;
		icode |= uint32_t(rs2) << 20;
		icode |= uint32_t(func7) << 25;
		instruction(icode);
		return *this;
	}

	RVOP& typeI(rv::op opcode, rv::func func3, size_t rd, size_t rs1, int32_t imm)
	{
		uint32_t icode = (uint32_t)opcode;
		icode |= uint32_t(rd) << 7;
		icode |= uint32_t(func3) << 12;
		icode |= uint32_t(rs1) << 15;
		icode |= uint32_t(imm) << 20;
		instruction(icode);
		return *this;
	}

	RVOP& typeU(rv::op opcode, size_t rd, uint32_t imm)
	{
		uint32_t icode = (uint32_t)opcode;
		icode |= uint32_t(rd) << 7;
		icode |= uint32_t(imm);
		instruction(icode);
		return *this;
	}

	RVOP& typeJ(rv::op opcode, size_t rd, int32_t imm)
	{
		uint32_t icode = (uint32_t)opcode;
		icode |= uint32_t(rd) << 7;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 12, 8) << 12;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 11, 1) << 20;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 1, 10) << 21;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 20, 1) << 31;
		instruction(icode);
		return *this;
	}

	RVOP& typeB(rv::op opcode, rv::func func3, int32_t imm, size_t rs1 = 0, size_t rs2 = 0)
	{
		uint32_t icode = (uint32_t)opcode;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 11, 1) << 7;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 1, 4) << 8;
		icode |= uint32_t(func3) << 12;
		icode |= uint32_t(rs1) << 15;
		icode |= uint32_t(rs2) << 20;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 5, 6) << 25;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 12, 1) << 31;
		instruction(icode);
		return *this;
	}

	RVOP& typeS(rv::op opcode, rv::func func3, int32_t imm, size_t rs1 = 0, size_t rs2 = 0)
	{
		uint32_t icode = (uint32_t)opcode;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 0, 5) << 7;
		icode |= uint32_t(func3) << 12;
		icode |= uint32_t(rs1) << 15;
		icode |= uint32_t(rs2) << 20;
		icode |= gtry::utils::bitfieldExtract(uint32_t(imm), 5, 7) << 25;
		instruction(icode);
		return *this;
	}

	UInt m_instructionWord = 32_b;
};

class RV32I_stub : public scl::riscv::RV32I
{
public:
	RV32I_stub()
	{
		m_discardResult = '0';

		m_IP = pinIn(32_b).setName("IP");
		m_IPnext = m_IP + 4;

		m_op.m_instructionWord = pinIn(32_b).setName("instruction");
		m_instr.decode(m_op.m_instructionWord);
		HCL_NAMED(m_instr);

		m_r1 = pinIn(32_b).setName("r1");
		m_r2 = pinIn(32_b).setName("r2");

		setupAlu();
		
		pinOut(m_aluResult.sum).setName("alu_sum");
		pinOut(m_aluResult.carry).setName("alu_carry");
		pinOut(m_aluResult.overflow).setName("alu_overflow");
		pinOut(m_aluResult.zero).setName("alu_zero");
		pinOut(m_aluResult.sign).setName("alu_sign");

		pinOut(m_setStall).setName("stall");
		pinOut(m_setResultValid).setName("result_valid");
		pinOut(m_setResult).setName("result");
		pinOut(m_setIP).setName("result_ip");

		m_setStall = '0';
		m_setResultValid = '0';
		m_setResult = "32b0";
		m_setIP = m_IPnext;
	}

	void setupSimu()
	{
		op().instruction(0);
		ip(0);
		r1(0);
		r2(0);

		if (m_avmm.readDataValid)
		{
			simu(*m_avmm.readDataValid) = '0';
			simu(*m_avmm.readData) = 0;
		}
	}

	RVOP& op() { return m_op; }

	scl::AvalonMM& setupMem()
	{
		m_avmm.readDataValid.emplace();
		m_avmm.readData = 32_b;
		mem(m_avmm);
		m_avmm.pinOut("avmm");
		return m_avmm;
	}

	scl::TileLinkUL& setupMemTileLink()
	{
		tileLinkInit(m_tlink, 32_b, 32_b, 2_b, 0_b);
		m_tlink <<= memTLink();
		pinOut(m_tlink, "dmem");

		DesignScope::get()->getCircuit().addSimulationProcess([=,this]()->SimProcess {
			simu(valid(*m_tlink.d)) = '0';
			simu(ready(m_tlink.a)) = '0';

			simu((*m_tlink.d)->opcode) = (size_t)scl::TileLinkD::AccessAck;
			simu((*m_tlink.d)->param) = 0;
			simu((*m_tlink.d)->size) = 2;
			simu((*m_tlink.d)->data) = 0;
			simu((*m_tlink.d)->error) = '0';
			co_return;
		});

		return m_tlink;
	}

	bool isStall() const { return simu(m_setStall) != '0'; }
	bool hasResult() const { return simu(m_setResultValid) != '0'; }
	uint32_t result() const { return (uint32_t)simu(m_setResult); }
	uint32_t ipNext() const { return (uint32_t)simu(m_setIP); }


	RV32I_stub& r1(uint32_t val) { simu(m_r1) = val; return *this; }
	RV32I_stub& r2(uint32_t val) { simu(m_r2) = val; return *this; }
	RV32I_stub& ip(uint32_t val) { simu(m_IP) = val; return *this; }

protected:
	virtual void setIP(const UInt& ip) { m_setIP = ip; }
	virtual void setResult(const UInt& result) { m_setResultValid = '1'; m_setResult = result; }
	virtual void setStall(const Bit& wait) { m_setStall = wait; }

	RVOP m_op;
	Bit m_setStall;
	Bit m_setResultValid;
	UInt m_setResult = 32_b;
	UInt m_setIP = 32_b;
	scl::AvalonMM m_avmm;
	scl::TileLinkUL m_tlink;

};


BOOST_FIXTURE_TEST_CASE(riscv_exec_arith, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.arith();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ 18055 };

		// ADD
		rv.op().add(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			rv.r1(opA).r2(opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == opA + opB);
		}

		// SUB
		rv.op().sub(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			rv.r1(opA).r2(opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == opA - opB);
		}

		// ADDI
		for (size_t i = 0; i < 64; ++i)
		{
			uint32_t opA = rng();
			int32_t opB = int32_t(rng()) >> (32 - 12);
			rv.op().addi(0, 0, opB);
			rv.r1(opA);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == opA + opB);
		}
	});

//	sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_arith.vcd" };
//	vcd.addAllPins();
//	vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_logic, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.logic();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		for (size_t i = 0; i < 256; ++i)
		{
			rv::func f = rng() % 2 ? rv::func::XOR : rv::func::AND;
			if (rng() % 3 == 0)
				f = rv::func::OR;

			uint32_t opA = rng();
			int32_t opB = (int32_t)rng();

			bool useImm = rng() % 2 == 0;
			if (useImm)
			{
				opB >>= 32 - 12;
				rv.r1(opA).op().typeI(rv::op::ARITHI, f, 0, 0, opB);
			}
			else
			{
				rv.r1(opA).r2(opB).op().typeR(rv::op::ARITH, f);
			}
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());

			if(f == rv::func::XOR)
				BOOST_TEST(rv.result() == (opA ^ opB));
			else if (f == rv::func::OR)
				BOOST_TEST(rv.result() == (opA | opB));
			else if (f == rv::func::AND)
				BOOST_TEST(rv.result() == (opA & opB));
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_logic.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 256);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_shift, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.shift();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// SLL
		rv.op().sll(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng() & 0x1F;
			rv.r1(opA).r2(opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == (opA << opB));
		}

		// SRL
		rv.op().srl(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng() & 0x1F;
			rv.r1(opA).r2(opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == (opA >> opB));
		}

		// SRA
		rv.op().sra(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng() & 0x1F;
			rv.r1(opA).r2(opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ((int32_t)opA >> opB));
		}

		// SLLI
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng() & 0x1F;
			rv.r1(opA).op().slli(0, 0, opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == (opA << opB));
		}

		// SRLI
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng() & 0x1F;
			rv.r1(opA).op().srli(0, 0, opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == (opA >> opB));
		}

		// SRAI
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng() & 0x1F;
			rv.r1(opA).op().srai(0, 0, opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ((int32_t)opA >> opB));
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_shift.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 32 * 6);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_setcmp, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.setcmp();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// SLT
		rv.op().slt(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			int32_t opA = int32_t(rng());
			int32_t opB = int32_t(rng());
			rv.r1((uint32_t)opA).r2((uint32_t)opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			uint32_t isTrue = opA < opB ? 1 : 0;
			BOOST_TEST(rv.result() == isTrue);
		}

		// SLTU
		rv.op().sltu(0, 0, 0);
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			rv.r1(opA).r2(opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			uint32_t isTrue = opA < opB ? 1 : 0;
			BOOST_TEST(rv.result() == isTrue);
		}

		// SLTI
		for (size_t i = 0; i < 32; ++i)
		{
			int32_t opA = int32_t(rng());
			int32_t opB = int32_t(rng()) >> (32 - 12);
			rv.r1((uint32_t)opA).op().slti(0, 0, opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			uint32_t isTrue = opA < opB ? 1 : 0;
			BOOST_TEST(rv.result() == isTrue);
		}

		// SLTUI
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			int32_t opB = int32_t(rng()) >> (32 - 12);
			rv.r1(opA).op().sltui(0, 0, opB);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			uint32_t isTrue = opA < uint32_t(opB) ? 1 : 0;
			BOOST_TEST(rv.result() == isTrue);
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_setcmp.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 32 * 4);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_lui, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.lui();
	rv.auipc();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// LUI
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng() & 0xFFFFF000u;
			rv.r1(rng()).r2(rng()).ip(rng());
			rv.op().lui(0, opA);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == opA);
		}

		// AUIPC
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng() & 0xFFFFF000u;
			uint32_t ip = rng();
			rv.r1(rng()).r2(rng());
			rv.ip(ip);
			rv.op().auipc(0, opA);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == opA + ip);
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_lui.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 32 * 2);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_jal, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.jal();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// JAL
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 21)) & ~1;

			rv.r1(rng()).r2(rng());
			rv.ip(ip);
			rv.op().jal(0, offset);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ip + 4);
			BOOST_TEST(rv.ipNext() == ip + offset);
		}

		// JALR
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t ip = rng();
			uint32_t opA = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			rv.r2(rng());
			rv.ip(ip).r1(opA);
			rv.op().jalr(0, 0, offset);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ip + 4);
			BOOST_TEST(rv.ipNext() == opA + offset);
		}
		});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_jal.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 32 * 2);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_csr_cycle, BoostUnitTestSimulationFixture)
{
	// default time resolution is 1us. set clock to 1MHz for 1:1 relationsship
	Clock clock({ .absoluteFrequency = 1'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.csrCycle();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };
		rv.ip(rng()).r1(rng()).r2(rng());
		rv.op().typeI(rv::op::SYSTEM, rv::func::CSRRW, 1, 0, 0xC00);

		co_await AfterClk(clock);
		BOOST_TEST(rv.hasResult());
		size_t start = rv.result();

		for (size_t i = 0; i < 14; ++i)
		{
			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() - start == i);

			rv.ip(rng()).r1(rng()).r2(rng());
			rv.op().typeI(rv::op::SYSTEM, (rv::func)(i % 7 + 1), 1, 0, 0xC00);
			co_await AfterClk(clock);
		}

		rv.ip(rng()).r1(rng()).r2(rng());
		rv.op().typeI(rv::op::SYSTEM, rv::func::CSRRW, 1, 0, 0xC80);
		co_await AfterClk(clock);
		BOOST_TEST(rv.hasResult());
		BOOST_TEST(rv.result() == 0);

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_csr_timer, BoostUnitTestSimulationFixture)
{
	// default time resolution is 1us. set clock to 1MHz for 1:1 relationsship
	Clock clock({ .absoluteFrequency = 1'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.csrTime();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };
		rv.ip(rng()).r1(rng()).r2(rng());
		rv.op().typeI(rv::op::SYSTEM, rv::func::CSRRW, 1, 0, 0xC01);

		co_await AfterClk(clock);
		BOOST_TEST(rv.hasResult());
		size_t start = rv.result();

		for (size_t i = 0; i < 14; ++i)
		{
			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() - start == i);

			rv.ip(rng()).r1(rng()).r2(rng());
			rv.op().typeI(rv::op::SYSTEM, (rv::func)(i % 7 + 1), 1, 0, 0xC01);
			co_await AfterClk(clock);
		}

		rv.ip(rng()).r1(rng()).r2(rng());
		rv.op().typeI(rv::op::SYSTEM, rv::func::CSRRW, 1, 0, 0xC81);
		co_await AfterClk(clock);
		BOOST_TEST(rv.hasResult());
		BOOST_TEST(rv.result() == 0);

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 128);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_csr_machine, BoostUnitTestSimulationFixture)
{
	// default time resolution is 1us. set clock to 1MHz for 1:1 relationsship
	Clock clock({ .absoluteFrequency = 1'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.csrMachineInformation(1, 2, 3, 4, 5);

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		for (size_t i = 1; i < 6; ++i)
		{
			rv.ip(rng()).r1(rng()).r2(rng());
			rv.op().typeI(rv::op::SYSTEM, rv::func::CSRRW, 1, 0, 0xF10 + int32_t(i));

			co_await AfterClk(clock);
			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == i);
		}

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 128);
}


BOOST_FIXTURE_TEST_CASE(riscv_exec_branch, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	rv.branch();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		std::vector<std::pair<uint32_t, uint32_t>> cases = {
			{ 0, 0 },	// equal
			{ 1u << 31, 1u << 31 }, // equal high bit set
			{ 0, 1u << 31 }, // high bit unequal
			{ 1u << 31, 0 }, // high bit unequal
			{ 1, 2 }, // small difference
			{ 2, 1 }, // small difference
			{ std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max() }, // big difference unsigned
			{ std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::min() }, // big difference unsigned
			{ std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max() }, // big difference signed
			{ std::numeric_limits<int32_t>::max(), std::numeric_limits<int32_t>::min() }, // big difference signed
		};
		for (size_t i = 0; i < 32; ++i)
			cases.push_back({ rng(), rng() });

		// BEQ
		for (auto c : cases)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 13)) & ~1;

			rv.r1(c.first).r2(c.second).ip(ip);
			rv.op().typeB(rv::op::BRANCH, rv::func::BEQ, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			if (c.first == c.second)
				BOOST_TEST(rv.ipNext() == ip + offset);
			else
				BOOST_TEST(rv.ipNext() == ip + 4);
		}

		// BNE
		for (auto c : cases)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 13)) & ~1;

			rv.r1(c.first).r2(c.second).ip(ip);
			rv.op().typeB(rv::op::BRANCH, rv::func::BNE, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			if (c.first != c.second)
				BOOST_TEST(rv.ipNext() == ip + offset);
			else
				BOOST_TEST(rv.ipNext() == ip + 4);
		}

		// BLT
		for (auto c : cases)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 13)) & ~1;

			rv.r1(c.first).r2(c.second).ip(ip);
			rv.op().typeB(rv::op::BRANCH, rv::func::BLT, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			if (int32_t(c.first) < int32_t(c.second))
				BOOST_TEST(rv.ipNext() == ip + offset);
			else
				BOOST_TEST(rv.ipNext() == ip + 4);
		}

		// BGE
		for (auto c : cases)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 13)) & ~1;

			rv.r1(c.first).r2(c.second).ip(ip);
			rv.op().typeB(rv::op::BRANCH, rv::func::BGE, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			if (int32_t(c.first) >= int32_t(c.second))
				BOOST_TEST(rv.ipNext() == ip + offset);
			else
				BOOST_TEST(rv.ipNext() == ip + 4);
		}

		// BLTU
		for (auto c : cases)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 13)) & ~1;

			rv.r1(c.first).r2(c.second).ip(ip);
			rv.op().typeB(rv::op::BRANCH, rv::func::BLTU, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			if (c.first < c.second)
				BOOST_TEST(rv.ipNext() == ip + offset);
			else
				BOOST_TEST(rv.ipNext() == ip + 4);
		}

		// BGEU
		for (auto c : cases)
		{
			uint32_t ip = rng();
			int32_t offset = (int32_t(rng()) >> (32 - 13)) & ~1;

			rv.r1(c.first).r2(c.second).ip(ip);
			rv.op().typeB(rv::op::BRANCH, rv::func::BGEU, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			if (c.first >= c.second)
				BOOST_TEST(rv.ipNext() == ip + offset);
			else
				BOOST_TEST(rv.ipNext() == ip + 4);
		}

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_store, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::AvalonMM& avmm = rv.setupMem();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// SW
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 3;
			opA -= alignmentFailure;

			rv.r1(opA).r2(opB).ip(rng());
			rv.op().typeS(rv::op::STORE, rv::func::WORD, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.write) == '1');
			BOOST_TEST(simu(*avmm.byteEnable) == 0xF);
			BOOST_TEST(simu(*avmm.writeData) == opB);
		}

		// SH
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 1;
			opA -= alignmentFailure;

			rv.r1(opA).r2(opB).ip(rng());
			rv.op().typeS(rv::op::STORE, rv::func::HALFWORD, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.write) == '1');

			size_t expectedByteEn = (opA + offset) % 4 < 2 ? 0x3 : 0xC;
			size_t expectedOffset = (opA + offset) % 4 < 2 ? 0 : 16;
			BOOST_TEST(simu(*avmm.byteEnable) == expectedByteEn);
			BOOST_TEST(((simu(*avmm.writeData) >> expectedOffset) & 0xFFFF) == (opB & 0xFFFF));
		}

		// SB
		for (size_t i = 0; i < 64; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			rv.r1(opA).r2(opB).ip(rng());
			rv.op().typeS(rv::op::STORE, rv::func::BYTE, offset);
			co_await AfterClk(clock);

			BOOST_TEST(!rv.hasResult());
			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.write) == '1');

			size_t expectedByteEn = 1ull << (opA + offset) % 4;
			size_t expectedOffset = ((opA + offset) % 4) * 8ull;
			BOOST_TEST(simu(*avmm.byteEnable) == expectedByteEn);
			BOOST_TEST(((simu(*avmm.writeData) >> expectedOffset) & 0xFF) == (opB & 0xFF));
		}
	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_store.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 32 * 4);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_tilelink_store, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::TileLinkUL& link = rv.setupMemTileLink();

	addSimulationProcess([&]()->SimProcess {
		while (true)
		{
			co_await AfterClk(clock);
			BOOST_TEST(!rv.hasResult());
		}
	});

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// SW
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 3;
			opA -= alignmentFailure;

			rv.r1(opA).r2(opB).ip(rng());
			rv.op().typeS(rv::op::STORE, rv::func::WORD, offset);

			simu(valid(*link.d)) = '0';
			simu(ready(link.a)) = '0';
			co_await WaitFor(0);

			BOOST_TEST(simu(link.a->address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::PutFullData);
			BOOST_TEST(simu(link.a->data) == opB);
			BOOST_TEST(simu(link.a->mask) == 0xF);
			while(rng() % 2 == 0)
			{
				co_await AfterClk(clock);
				BOOST_TEST(rv.isStall());
				BOOST_TEST(simu(valid(link.a)) == '1');
			}

			simu(ready(link.a)) = '1';
			co_await WaitFor(0);

			while (rng() % 2 == 0)
			{
				co_await AfterClk(clock);
				BOOST_TEST(simu(valid(link.a)) == '0');
				BOOST_TEST(rv.isStall());
			}
			simu(valid(*link.d)) = '1';
			co_await AfterClk(clock);
			BOOST_TEST(!rv.isStall());

		}

		simu(valid(*link.d)) = '0';
		simu(ready(link.a)) = '0';
		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_tilelink_byte_store, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::TileLinkUL& link = rv.setupMemTileLink();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };
		for (size_t i = 0; i < 8; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			rv.r1(opA).r2(opB).ip(rng());
			rv.op().typeS(rv::op::STORE, rv::func::BYTE, offset);
			co_await AfterClk(clock);
			
			BOOST_TEST(simu(valid(link.a)) == '1');
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::PutFullData);
			BOOST_TEST(simu(link.a->address) == opA + offset);
			BOOST_TEST(simu(link.a->size) == 0);

			size_t expectedByteEn = 1ull << (opA + offset) % 4;
			size_t expectedOffset = ((opA + offset) % 4) * 8ull;
			BOOST_TEST(simu(link.a->mask) == expectedByteEn);
			BOOST_TEST(((simu(link.a->data) >> expectedOffset) & 0xFF) == (opB & 0xFF));
		}

		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_tilelink_half_store, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::TileLinkUL& link = rv.setupMemTileLink();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };
		for (size_t i = 0; i < 8; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 1;
			opA -= alignmentFailure;

			rv.r1(opA).r2(opB).ip(rng());
			rv.op().typeS(rv::op::STORE, rv::func::HALFWORD, offset);
			co_await AfterClk(clock);

			BOOST_TEST(simu(valid(link.a)) == '1');
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::PutFullData);
			BOOST_TEST(simu(link.a->address) == opA + offset);
			BOOST_TEST(simu(link.a->size) == 1);

			size_t expectedByteEn = (opA + offset) % 4 < 2 ? 0x3 : 0xC;
			size_t expectedOffset = (opA + offset) % 4 < 2 ? 0 : 16;
			BOOST_TEST(simu(link.a->mask) == expectedByteEn);
			BOOST_TEST(((simu(link.a->data) >> expectedOffset) & 0xFF) == (opB & 0xFF));
		}

		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_tilelink_byte_load, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::TileLinkUL& link = rv.setupMemTileLink();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		co_await WaitFor(0);
		simu(ready(link.a)) = '1';
		simu(valid(*link.d)) = '1';
		simu((*link.d)->size) = 0;

		std::mt19937 rng{ std::random_device{}() };
		for (size_t i = 0; i < 8; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::BYTEU, 0, 0, offset);

			uint32_t readData = rng();
			simu((*link.d)->data) = readData;
			co_await AfterClk(clock);

			BOOST_TEST(simu(valid(link.a)) == '1');
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::Get);
			BOOST_TEST(simu(link.a->address) == opA + offset);
			BOOST_TEST(simu(link.a->size) == 0);

			size_t expectedByteEn = 1ull << (opA + offset) % 4;
			size_t expectedOffset = ((opA + offset) % 4) * 8ull;
			BOOST_TEST(simu(link.a->mask) == expectedByteEn);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ((readData >> expectedOffset) & 0xFF));
			BOOST_TEST(!rv.isStall());
		}

		for (size_t i = 0; i < 8; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::BYTE, 0, 0, offset);

			int32_t readData = rng();
			simu((*link.d)->data) = (uint32_t) readData;
			co_await AfterClk(clock);

			BOOST_TEST(simu(valid(link.a)) == '1');
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::Get);
			BOOST_TEST(simu(link.a->address) == opA + offset);
			BOOST_TEST(simu(link.a->size) == 0);

			size_t expectedByteEn = 1ull << (opA + offset) % 4;
			size_t expectedOffset = ((opA + offset) % 4) * 8ull;
			BOOST_TEST(simu(link.a->mask) == expectedByteEn);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ((readData >> expectedOffset) << 24 >> 24));
			BOOST_TEST(!rv.isStall());
		}

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_tilelink_half_load, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::TileLinkUL& link = rv.setupMemTileLink();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		co_await WaitFor(0);
		simu(ready(link.a)) = '1';
		simu(valid(*link.d)) = '1';
		simu((*link.d)->size) = 0;

		std::mt19937 rng{ std::random_device{}() };
		for (size_t i = 0; i < 8; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 1;
			opA -= alignmentFailure;

			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::HALFWORDU, 0, 0, offset);

			uint32_t readData = rng();
			simu((*link.d)->data) = readData;
			co_await AfterClk(clock);

			BOOST_TEST(simu(valid(link.a)) == '1');
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::Get);
			BOOST_TEST(simu(link.a->address) == opA + offset);
			BOOST_TEST(simu(link.a->size) == 1);

			size_t expectedByteEn = (opA + offset) % 4 < 2 ? 0x3 : 0xC;
			size_t expectedOffset = (opA + offset) % 4 < 2 ? 0 : 16;
			BOOST_TEST(simu(link.a->mask) == expectedByteEn);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ((readData >> expectedOffset) & 0xFFFF));
			BOOST_TEST(!rv.isStall());
		}

		for (size_t i = 0; i < 8; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 1;
			opA -= alignmentFailure;

			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::HALFWORD, 0, 0, offset);

			int32_t readData = rng();
			simu((*link.d)->data) = (uint32_t) readData;
			co_await AfterClk(clock);

			BOOST_TEST(simu(valid(link.a)) == '1');
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::Get);
			BOOST_TEST(simu(link.a->address) == opA + offset);
			BOOST_TEST(simu(link.a->size) == 1);

			size_t expectedByteEn = (opA + offset) % 4 < 2 ? 0x3 : 0xC;
			size_t expectedOffset = (opA + offset) % 4 < 2 ? 0 : 16;
			BOOST_TEST(simu(link.a->mask) == expectedByteEn);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == ((readData >> expectedOffset) << 16 >> 16));
			BOOST_TEST(!rv.isStall());
		}

		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}

BOOST_FIXTURE_TEST_CASE(riscv_exec_tilelink_load, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::TileLinkUL& link = rv.setupMemTileLink();

	addSimulationProcess([&]()->SimProcess {
		co_await WaitFor(0);
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		simu((*link.d)->opcode) = (size_t)scl::TileLinkD::AccessAckData;

		// LW
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t opB = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 3;
			opA -= alignmentFailure;

			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::WORD, 0, 0, offset);

			simu(valid(*link.d)) = '0';
			simu(ready(link.a)) = '0';
			simu((*link.d)->data).invalidate();
			simu((*link.d)->error).invalidate();
			co_await WaitFor(0);

			BOOST_TEST(simu(link.a->address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(link.a->opcode) == (size_t)scl::TileLinkA::Get);
			BOOST_TEST(simu(link.a->mask) == 0xF);
			while (rng() % 2 == 0)
			{
				co_await AfterClk(clock);
				BOOST_TEST(rv.isStall());
				BOOST_TEST(simu(valid(link.a)) == '1');
			}
			simu(ready(link.a)) = '1';
			co_await WaitFor(0);

			while (rng() % 2 == 0)
			{
				co_await AfterClk(clock);
				BOOST_TEST(simu(valid(link.a)) == '0');
				BOOST_TEST(rv.isStall());
			}


			uint32_t readData = rng();
			bool error = rng() % 16 == 0;
			if (error)
			{
				simu((*link.d)->error) = '1';
			}
			else
			{
				simu((*link.d)->error) = '0';
				simu((*link.d)->data) = readData;
			}

			simu(valid(*link.d)) = '1';
			co_await AfterClk(clock);
			BOOST_TEST(!rv.isStall());
			BOOST_TEST(rv.hasResult());

			BOOST_TEST(rv.result() == (error ? 0xFFFF'FFFFu : readData));
		}

		simu(valid(*link.d)) = '0';
		simu(ready(link.a)) = '0';
		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
}
BOOST_FIXTURE_TEST_CASE(riscv_exec_load, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	RV32I_stub rv;
	scl::AvalonMM& avmm = rv.setupMem();

	addSimulationProcess([&]()->SimProcess {
		rv.setupSimu();

		std::mt19937 rng{ std::random_device{}() };

		// LW
		simu(*avmm.readDataValid) = '1';
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 3;
			opA -= alignmentFailure;			

			simu(*avmm.readData) = data;
			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::WORD, 0, 0, offset);
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == data);
			
			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.read) == '1');
			BOOST_TEST(simu(*avmm.byteEnable) == 0xF);
		}

		// LH
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 1;
			opA -= alignmentFailure;

			simu(*avmm.readData) = data;
			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::HALFWORD, 0, 0, offset);
			co_await AfterClk(clock);

			size_t expectedOffset = (opA + offset) % 4 < 2 ? 0 : 16;

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == int32_t(data << (16 - expectedOffset)) >> 16);

			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.read) == '1');
			size_t expectedByteEn = (opA + offset) % 4 < 2 ? 0x3 : 0xC;
			BOOST_TEST(simu(*avmm.byteEnable) == expectedByteEn);
		}

		// LHU
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 1;
			opA -= alignmentFailure;			

			simu(*avmm.readData) = data;
			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::HALFWORDU, 0, 0, offset);
			co_await AfterClk(clock);

			size_t expectedOffset = (opA + offset) % 4 < 2 ? 0 : 16;

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == (data << (16 - expectedOffset)) >> 16);

			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.read) == '1');
			size_t expectedByteEn = (opA + offset) % 4 < 2 ? 0x3 : 0xC;
			BOOST_TEST(simu(*avmm.byteEnable) == expectedByteEn);
		}

		// LB
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			simu(*avmm.readData) = data;
			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::BYTE, 0, 0, offset);
			co_await AfterClk(clock);

			size_t expectedOffset = ((opA + offset) % 4) * 8ull;

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == int32_t(data << (24 - expectedOffset)) >> 24);

			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.read) == '1');
			size_t expectedByteEn = 1ull << (opA + offset) % 4;
			BOOST_TEST(simu(*avmm.byteEnable) == expectedByteEn);
		}

		// LBU
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			simu(*avmm.readData) = data;
			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::BYTEU, 0, 0, offset);
			co_await AfterClk(clock);

			size_t expectedOffset = ((opA + offset) % 4) * 8ull;

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == (data << (24 - expectedOffset)) >> 24);

			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.read) == '1');
			size_t expectedByteEn = 1ull << (opA + offset) % 4;
			BOOST_TEST(simu(*avmm.byteEnable) == expectedByteEn);
		}

		// LW delayed
		for (size_t i = 0; i < 32; ++i)
		{
			uint32_t delay = (rng() & 0xF) + 1;
			uint32_t opA = rng();
			uint32_t data = rng();
			int32_t offset = int32_t(rng()) >> (32 - 12);

			uint32_t alignmentFailure = (opA + offset) & 3;
			opA -= alignmentFailure;

			simu(*avmm.readDataValid) = '0';
			simu(*avmm.readData) = data;
			rv.r1(opA).r2(rng()).ip(rng());
			rv.op().typeI(rv::op::LOAD, rv::func::WORD, 0, 0, offset);
			co_await WaitFor(0);
			BOOST_TEST(simu(*avmm.read) == '1');

			for (size_t j = 0; j < delay; ++j)
			{
				co_await WaitFor(0);
				BOOST_TEST(rv.isStall());
				co_await AfterClk(clock);
				co_await WaitFor(0);
				BOOST_TEST(simu(*avmm.read) == '0');
			}

			simu(*avmm.readDataValid) = '1';
			co_await AfterClk(clock);

			BOOST_TEST(rv.hasResult());
			BOOST_TEST(rv.result() == data);
			BOOST_TEST(!rv.isStall());
			BOOST_TEST(simu(avmm.address) == ((opA + offset) & ~3));
			BOOST_TEST(simu(*avmm.byteEnable) == 0xF);
		}

	});

	//sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "riscv_load.vcd" };
	//vcd.addAllPins();
	//vcd.addAllNamedSignals();

	design.postprocess();
	runTicks(clock.getClk(), 512);
}

static const unsigned char gcd_bin[] = {
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
BOOST_FIXTURE_TEST_CASE(riscv_single_cycle, BoostUnitTestSimulationFixture)
{
	Clock clock({ .absoluteFrequency = 100'000'000 });
	ClockScope clkScp(clock);

	scl::riscv::SingleCycleI rv(8_b, 32_b);
	Memory<UInt>& imem = rv.fetch();
	imem.fillPowerOnState(sim::createDefaultBitVectorState(sizeof(gcd_bin) * 8, gcd_bin));
	rv.fetchOperands();

	scl::AvalonMM avmm;
	avmm.readLatency = 1;
	avmm.readData = 32_b;

	avmm.read = Bit{};
	avmm.readDataValid = reg(*avmm.read, '0');
	rv.execute();
	rv.mem(avmm);

	std::random_device rng;
	
	Memory<UInt> dmem(1024, 32_b);
	std::vector<unsigned char> dmemData(4096, 0);
	dmemData[0] = 15; // (rng() & 0x3F) + 1;
	dmemData[4] = 5; // (rng() & 0x3F) + 1;
	dmem.fillPowerOnState(sim::createDefaultBitVectorState(dmemData.size()*8, dmemData.data()));
	auto dport = dmem[avmm.address(2, 10_b)];
	
	IF(*avmm.write)
		dport = *avmm.writeData;
	*avmm.readData = reg(dport.read(), {.allowRetimingBackward=true});
	avmm.readData->setName("avmm_readdata");
	avmm.read->setName("avmm_read");
	avmm.readDataValid->setName("avmm_readdatavalid");

	pinOut(avmm.address).setName("avmm_address");
	pinOut(*avmm.write).setName("avmm_write");
	pinOut(*avmm.writeData).setName("avmm_writedata");

	uint64_t expectedResult = scl::gcd(dmemData[0], dmemData[4]);
	size_t timeout = std::max(dmemData[0], dmemData[4]) * 4ull + 32;
	addSimulationProcess([&]()->SimProcess {
	
		bool found = false;
		for (size_t i = 0; i < timeout; ++i)
		{
			co_await AfterClk(clock);
			if (simu(*avmm.write))
			{
				BOOST_TEST(simu(avmm.address) == 8);
				BOOST_TEST(simu(*avmm.writeData) == expectedResult);
				found = true;
			}
		}
		BOOST_TEST(found);

	});

	design.postprocess();
	runTicks(clock.getClk(), (uint32_t)timeout + 2);
}

//#include <gatery/scl/riscv/RiscVAssembler.h>

BOOST_FIXTURE_TEST_CASE(riscv_dual_cycle, BoostUnitTestSimulationFixture)
{
	//scl::riscv::assembler::printCode(std::cout, { (const uint32_t*)gcd_bin, sizeof(gcd_bin)/4 }, 0);

	Clock clock({
		.absoluteFrequency = 100'000'000,
		.resetType = ClockConfig::ResetType::NONE
	});
	ClockScope clkScp(clock);

	scl::riscv::DualCycleRV rv(8_b, 32_b);
	Memory<UInt>& imem = rv.fetch();
	imem.fillPowerOnState(sim::createDefaultBitVectorState(sizeof(gcd_bin)*8, gcd_bin));

	scl::AvalonMM avmm;
	avmm.readLatency = 1;
	avmm.readData = 32_b;

	avmm.read = Bit{};
	avmm.readDataValid = reg(*avmm.read, '0');
	rv.execute();
	rv.mem(avmm);

	std::random_device rng;

	Memory<UInt> dmem(1024, 32_b);
	std::vector<unsigned char> dmemData(4096, 0);
	dmemData[0] = (rng() & 0x3F) + 1;
	dmemData[4] = (rng() & 0x3F) + 1;
	dmem.fillPowerOnState(sim::createDefaultBitVectorState(dmemData.size()*8, dmemData.data()));
	auto dport = dmem[avmm.address(2, 10_b)];

	IF(*avmm.write)
		dport = *avmm.writeData;
	*avmm.readData = reg(dport.read(), {.allowRetimingBackward=true});
	avmm.readData->setName("avmm_readdata");
	avmm.read->setName("avmm_read");
	avmm.readDataValid->setName("avmm_readdatavalid");

	pinOut(avmm.address).setName("avmm_address");
	pinOut(*avmm.write).setName("avmm_write");
	pinOut(*avmm.writeData).setName("avmm_writedata");

	uint64_t expectedResult = scl::gcd(dmemData[0], dmemData[4]);
	// size_t timeout = std::max(dmemData[0], dmemData[4]) * 8ull + 32;
	bool found = false;
	addSimulationProcess([&]()->SimProcess {
		while (true)
		{
			co_await AfterClk(clock);
			if (simu(*avmm.write))
			{
				BOOST_TEST(simu(avmm.address) == 8);
				BOOST_TEST(simu(*avmm.writeData) == expectedResult);
				found = true;
				break;
			}
		}
		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 4096);
	BOOST_TEST(found);
}

BOOST_FIXTURE_TEST_CASE(riscv_dual_cycle_itlink, BoostUnitTestSimulationFixture)
{
	Clock clock({
		.absoluteFrequency = 100'000'000,
		.resetType = ClockConfig::ResetType::NONE
	});
	ClockScope clkScp(clock);

	scl::riscv::DualCycleRV rv(8_b, 32_b);
	scl::TileLinkUL imem = rv.fetchTileLink();
	pinOut(imem, "imem");

	addSimulationProcess([&]()->SimProcess {

		const uint32_t seed = std::random_device{}();
		std::mt19937 rng{ seed };
		//std::cout << "seed " << seed << '\n';

		simu(ready(imem.a)) = '0';
		simu(valid(*imem.d)) = '0';		
		simu((*imem.d)->opcode) = (size_t)scl::TileLinkD::AccessAckData;
		simu((*imem.d)->param) = 0;
		simu((*imem.d)->size) = 2;
		simu((*imem.d)->sink) = 0;

		co_await AfterClk(clock);

		fork(scl::validate(imem, clock));

		size_t hasRequest = 0;
		size_t requestAddress = 0xCC;

		while (true)
		{
			simu(ready(imem.a)) = '0';
			simu(valid(*imem.d)) = '0';
			if ((hasRequest & 1))
			{
				simu(valid(*imem.d)) = '1';
				BOOST_TEST(requestAddress < sizeof(gcd_bin));
				if (requestAddress < sizeof(gcd_bin))
				{
					const uint32_t* code = (const uint32_t*)gcd_bin;
					const uint32_t instruction = code[requestAddress / 4];
					simu((*imem.d)->data) = instruction;
					simu((*imem.d)->error) = '0';
				}
				else
				{
					simu((*imem.d)->error) = '1';
					simu((*imem.d)->data).invalidate();
				}
				simu((*imem.d)->source) = simu(imem.a->source);
			}
			hasRequest >>= 1;
			co_await WaitFor(0);

			if (simu(valid(imem.a)))
			{
				if (rng() % 2)
				{
					simu(ready(imem.a)) = '1';
					hasRequest = 1ull << (rng() % 6);
					requestAddress = simu(imem.a->address);
				}
			}
			co_await OnClk(clock);
		}
	});

	scl::AvalonMM avmm;
	avmm.readLatency = 1;
	avmm.readData = 32_b;

	avmm.read = Bit{};
	avmm.readDataValid = reg(*avmm.read, '0');
	rv.execute();
	rv.mem(avmm);

	std::random_device rng;

	Memory<UInt> dmem(1024, 32_b);
	std::vector<unsigned char> dmemData(4096, 0);
	dmemData[0] = (rng() & 0x3F) + 1;
	dmemData[4] = (rng() & 0x3F) + 1;
	//std::cout << "in=" << (size_t)dmemData[0] << ',' << (size_t)dmemData[4] << '\n';
	dmem.fillPowerOnState(sim::createDefaultBitVectorState(dmemData.size()*8, dmemData.data()));
	auto dport = dmem[avmm.address(2, 10_b)];

	IF(*avmm.write)
		dport = *avmm.writeData;
	*avmm.readData = reg(dport.read(), { .allowRetimingBackward = true });
	avmm.readData->setName("avmm_readdata");
	avmm.read->setName("avmm_read");
	avmm.readDataValid->setName("avmm_readdatavalid");

	pinOut(avmm.address).setName("avmm_address");
	pinOut(*avmm.write).setName("avmm_write");
	pinOut(*avmm.writeData).setName("avmm_writedata");

	uint64_t expectedResult = scl::gcd(dmemData[0], dmemData[4]);
	// size_t timeout = std::max(dmemData[0], dmemData[4]) * 8ull + 32;
	bool found = false;
	addSimulationProcess([&]()->SimProcess {
		while (true)
		{
			co_await AfterClk(clock);
			if (simu(*avmm.write))
			{
				BOOST_TEST(simu(avmm.address) == 8);
				BOOST_TEST(simu(*avmm.writeData) == expectedResult);
				found = true;
				break;
			}
		}
		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	runTicks(clock.getClk(), 1024 * 16);
	BOOST_TEST(found);
}


BOOST_FIXTURE_TEST_CASE(riscv_dual_cycle_itlink_sharedmem, BoostUnitTestSimulationFixture)
{
	std::random_device rng;

	Clock clock({
		.absoluteFrequency = 100'000'000,
		.resetType = ClockConfig::ResetType::NONE
		});
	ClockScope clkScp(clock);

	scl::TileLinkHub<scl::TileLinkUL> hub;

	size_t instructionMemOffset = 0x1'0000;

	scl::riscv::DualCycleRV rv;
	//TODO remove register and find cyclic dependency
	hub.attachSource(regDecouple(rv.fetchTileLink(instructionMemOffset)));
	rv.execute();
	hub.attachSource(rv.memTLink());

	size_t opA = (rng() & 0x3F) + 1;
	size_t opB = (rng() & 0x3F) + 1;
	//std::cout << "in=" << opA << ',' << opB << '\n';

	scl::TileLinkUL dmemBus;
	{
		Memory<BVec> dmem(1024, 32_b);
		std::vector<unsigned char> dmemData(4096, 0);
		dmemData[0] = (uint8_t)opA;
		dmemData[4] = (uint8_t)opB;
		dmem.fillPowerOnState(sim::createDefaultBitVectorState(dmemData.size()*8, dmemData.data()));

		scl::tileLinkInit(dmemBus, 12_b, 32_b, 2_b, hub.sourceWidth());
		HCL_NAMED(dmemBus);
		dmem <<= dmemBus;
		HCL_NAMED(dmemBus);
		hub.attachSink(dmemBus, 0);

		pinOut(valid(dmemBus.a), "dmem_valid");
		pinOut(ready(dmemBus.a), "dmem_ready");
		pinOut(*dmemBus.a, "dmem");
	}

	{
		Memory<BVec> imem(64, 32_b);
		imem.fillPowerOnState(sim::createDefaultBitVectorState(sizeof(gcd_bin)*8, gcd_bin));

		scl::TileLinkUL imemBus;
		scl::tileLinkInit(imemBus, 8_b, 32_b, 2_b, hub.sourceWidth());
		HCL_NAMED(imemBus);
		imem <<= imemBus;
		HCL_NAMED(imemBus);
		hub.attachSink(imemBus, instructionMemOffset);
	}
	hub.generate();

	uint64_t expectedResult = scl::gcd(opA, opB);
	bool found = false;
	addSimulationProcess([&]()->SimProcess {
		while (!found)
		{
			co_await AfterClk(clock);
			if (simu(valid(dmemBus.a)) == '1' && 
				simu(ready(dmemBus.a)) == '1' &&
				simu(dmemBus.a->opcode) == (size_t)scl::TileLinkA::PutFullData)
			{
				BOOST_TEST(simu(dmemBus.a->address) == 8);
				BOOST_TEST(simu(dmemBus.a->mask) == 0xF);
				BOOST_TEST(simu(dmemBus.a->data) == expectedResult);
				found = true;
			}
		}

		co_await AfterClk(clock);
		stopTest();
	});

	design.postprocess();
	//dbg::vis();
	runTicks(clock.getClk(), 1024);
	BOOST_TEST(found);
}

#if __has_include(<external/rvcc/src/defs.c>)
BOOST_AUTO_TEST_CASE(rvcc_test)
{
	std::string code = 
 R"(int main(int a, int b)
	{
		while (a != b) {
			if (a > b)
				a -= b;
			else
				b -= a;
		}
		return a;
	})";

	std::vector<uint32_t> bin = scl::riscv::rvcc(code);
	BOOST_TEST(!bin.empty());
}

#endif

#if 0
extern gtry::hlim::NodeGroup* dbg_group;

BOOST_FIXTURE_TEST_CASE(riscv_embedded_system_builder, BoostUnitTestSimulationFixture)
{
	Clock clock(ClockConfig{}.absoluteFrequency(10'000'000).setName("clock").setResetHighActive(false));
	ClockScope clkScp(clock);

	{
		std::filesystem::path elfPath = "C:/Users/mio.SYNOGATE/Downloads/riscv64-unknown-elf-toolchain-10.2.0-2020.12.8-x86_64-w64-mingw32/bin/a.out";

		scl::riscv::EmbeddedSystemBuilder esb;
		esb.addCpu(elfPath, 512_B);

		Bit uart_rx = pinIn().setName("uart_rx");
		Bit uart_tx = esb.addUART(0x8000'0000, 115200, uart_rx);
		//Bit uart_tx = esb.addUART(0x8000'0000, 5'000'000, uart_rx);
		pinOut(uart_tx).setName("uart_tx");

		addSimulationProcess([=]()->SimProcess {
			simu(uart_rx) = 0;
			co_return;
		});
	}



	sim::VCDSink vcd{ design.getCircuit(), getSimulator(), "export/rv32i_esb/rv32i_esb.vcd" };
	vcd.addAllPins();
	vcd.addAllNamedSignals();

	design.postprocess();
	//design.visualize("export/rv32i_esb/rv32i_esb", dbg_group);
	vhdl::VHDLExport vhdl("export/rv32i_esb/rv32i_esb.vhd");
	vhdl(design.getCircuit());


	runTicks(clock.getClk(), 2048);

}

#endif

// hello world demo
#if 0

BOOST_FIXTURE_TEST_CASE(riscv_single_cycle_export, BoostUnitTestSimulationFixture)
{
	unsigned char linked_text[] = {
	  0x13, 0x00, 0x00, 0x00, 0xb7, 0x07, 0x00, 0x80, 0x03, 0xa1, 0x07, 0x10,
	  0xef, 0x00, 0xc0, 0x02, 0x6f, 0x00, 0x00, 0x00, 0x23, 0x20, 0xa0, 0x00,
	  0x13, 0x07, 0x00, 0x00, 0x6f, 0x00, 0xc0, 0x00, 0x13, 0x00, 0x00, 0x00,
	  0x13, 0x07, 0x17, 0x00, 0xb7, 0x27, 0x00, 0x00, 0x93, 0x87, 0xf7, 0x70,
	  0xe3, 0xf8, 0xe7, 0xfe, 0x67, 0x80, 0x00, 0x00, 0x13, 0x01, 0x01, 0xff,
	  0x23, 0x26, 0x11, 0x00, 0x6f, 0x00, 0x80, 0x01, 0x13, 0x07, 0x17, 0x00,
	  0xb7, 0xc7, 0x2d, 0x00, 0x93, 0x87, 0xf7, 0x6b, 0xe3, 0xfa, 0xe7, 0xfe,
	  0x13, 0x00, 0x00, 0x00, 0x13, 0x05, 0x80, 0x04, 0xef, 0xf0, 0x9f, 0xfb,
	  0x13, 0x05, 0x50, 0x06, 0xef, 0xf0, 0x1f, 0xfb, 0x13, 0x05, 0xc0, 0x06,
	  0xef, 0xf0, 0x9f, 0xfa, 0x13, 0x05, 0xc0, 0x06, 0xef, 0xf0, 0x1f, 0xfa,
	  0x13, 0x05, 0xf0, 0x06, 0xef, 0xf0, 0x9f, 0xf9, 0x13, 0x05, 0x00, 0x02,
	  0xef, 0xf0, 0x1f, 0xf9, 0x13, 0x05, 0x70, 0x05, 0xef, 0xf0, 0x9f, 0xf8,
	  0x13, 0x05, 0xf0, 0x06, 0xef, 0xf0, 0x1f, 0xf8, 0x13, 0x05, 0x20, 0x07,
	  0xef, 0xf0, 0x9f, 0xf7, 0x13, 0x05, 0xc0, 0x06, 0xef, 0xf0, 0x1f, 0xf7,
	  0x13, 0x05, 0x40, 0x06, 0xef, 0xf0, 0x9f, 0xf6, 0x13, 0x05, 0x10, 0x02,
	  0xef, 0xf0, 0x1f, 0xf6, 0x13, 0x05, 0xa0, 0x00, 0xef, 0xf0, 0x9f, 0xf5,
	  0x13, 0x07, 0x00, 0x00, 0x6f, 0xf0, 0x5f, 0xf8
	};
	unsigned int linked_text_len = 200;
	{
		Clock clock(ClockConfig{}.absoluteFrequency(10'000'000).setName("clock").setResetHighActive(false));
		ClockScope clkScp(clock);

		//scl::riscv::SingleCycleI rv(8_b, 32_b);
		scl::riscv::DualCycleRV rv(8_b, 32_b);
		Memory<UInt>& imem = rv.fetch();
		imem.fillPowerOnState(sim::createDefaultBitVectorState(linked_text_len*8, linked_text));
		//rv.fetchOperands();

		scl::AvalonMM avmm;
		avmm.readLatency = 1;
		avmm.readData = 32_b;
		avmm.read = Bit{};
		avmm.readDataValid = reg(*avmm.read, '0');
		rv.execute();
		rv.mem(avmm, true, true);

		Memory<UInt> dmem(1024, 32_b);
		dmem.noConflicts();
		std::vector<unsigned char> dmemData(4096, 0);
		//dmemData[0] = (rng() & 0x3F) + 1;
		//dmemData[4] = (rng() & 0x3F) + 1;
		//dmem.setPowerOnState(sim::createDefaultBitVectorState(dmemData.size()*8, dmemData.data()));
		auto dport = dmem[avmm.address(2, 10_b)];

		scl::UART::Stream uart_tx;
		uart_tx.valid = '0';
		uart_tx.data = (*avmm.writeData)(0, 8_b);

		*avmm.readData = reg(dport.read());
		IF(*avmm.write)
		{
			IF(avmm.address == 0)
			{
				pinOut(reg(~(*avmm.writeData)(0, 8_b), 0)).setName("led");
				uart_tx.valid = '1';
			}

			dport = *avmm.writeData;
		}

		//IF(avmm.address == 0)
		//	avmm.readData = zext(reg(pinIn(2_b).setName("sw")));
		//avmm.readData = reg(*avmm.readData, 0);

		scl::UART uart;
		Bit uart_tx_pin = uart.send(uart_tx);
		pinOut(uart_tx_pin).setName("uart_tx");
	}

	design.postprocess();
	//design.visualize("scrv32i");

	vhdl::VHDLExport vhdl("rv32i_gcd/rv32i_gcd.vhd");
	vhdl(design.getCircuit());

}

#endif
