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
#include "gatery/scl_pch.h"
#include "riscv.h"
#include "../Adder.h"
#include "../utils/OneHot.h"
#include "../tilelink/tilelink.h"
#include "../Counter.h"

void gtry::scl::riscv::Instruction::decode(const UInt& inst)
{
	instruction = inst;

	opcode = inst(2, 5_b);
	rd = inst(7, 5_b);
	func3 = inst(12, 3_b);
	rs1 = inst(15, 5_b);
	rs2 = inst(20, 5_b);
	func7 = inst(25, 7_b);

	immI = sext(inst(20, 12_b));
	immS = sext(cat(inst(25, 7_b), inst(7, 5_b)));
	immB = sext(cat(inst.msb(), inst[7], inst(25, 6_b), inst(8, 4_b), '0'));
	immU = cat(inst(20, 12_b), inst(12, 8_b), "12b0");
	immJ = sext(cat(inst.msb(), inst(12, 8_b), inst[20], inst(25, 6_b), inst(21, 4_b), '0'));

	// for waveform debugging
	name = 0;
	IF(opcode == "b01101")
		name = "32sLUI";
	IF(opcode == "b00101")
		name = "32sAUIP";
	IF(opcode == "b11011")
		name = "32sJAL";
	IF(opcode == "b11001" & func3 == 0)
		name = "32sJALR";
	IF(opcode == "b11000" & func3 == 0)
		name = "32sBEQ";
	IF(opcode == "b11000" & func3 == 1)
		name = "32sBNE";
	IF(opcode == "b11000" & func3 == 4)
		name = "32sBLT";
	IF(opcode == "b11000" & func3 == 5)
		name = "32sBGE";
	IF(opcode == "b11000" & func3 == 6)
		name = "32sBLTU";
	IF(opcode == "b11000" & func3 == 7)
		name = "32sBGEU";

	IF(opcode == "b00000" & func3 == 0)
		name = "32sLB";
	IF(opcode == "b00000" & func3 == 1)
		name = "32sLH";
	IF(opcode == "b00000" & func3 == 2)
		name = "32sLW";
	IF(opcode == "b00000" & func3 == 4)
		name = "32sLBU";
	IF(opcode == "b00000" & func3 == 5)
		name = "32sLHU";

	IF(opcode == "b01000" & func3 == 0)
		name = "32sSB";
	IF(opcode == "b01000" & func3 == 1)
		name = "32sSH";
	IF(opcode == "b01000" & func3 == 2)
		name = "32sSW";

	IF(opcode == "b00100" & func3 == 0)
		name = "32sADDI";
	IF(opcode == "b00100" & func3 == 1)
		name = "32sSLLI";
	IF(opcode == "b00100" & func3 == 2)
		name = "32sSLTI";
	IF(opcode == "b00100" & func3 == 3)
		name = "32sSLTU";
	IF(opcode == "b00100" & func3 == 4)
		name = "32sXORI";
	IF(opcode == "b00100" & func3 == 5)
		name = "32sSRLI";
	IF(opcode == "b00100" & func3 == 5 & func7[5])
		name = "32sSRAI";
	IF(opcode == "b00100" & func3 == 6)
		name = "32sORI";
	IF(opcode == "b00100" & func3 == 7)
		name = "32sANDI";
	IF(opcode == "b00100" & rd == 0)
		name = "32sNOOP";

	IF(opcode == "b01100" & func3 == 0)
		name = "32sADD";
	IF(opcode == "b01100" & func3 == 0 & func7[5])
		name = "32sSUB";
	IF(opcode == "b01100" & func3 == 1)
		name = "32sSLL";
	IF(opcode == "b01100" & func3 == 2)
		name = "32sSLT";
	IF(opcode == "b01100" & func3 == 3)
		name = "32sSLTU";
	IF(opcode == "b01100" & func3 == 4)
		name = "32sXOR";
	IF(opcode == "b01100" & func3 == 5)
		name = "32sSRL";
	IF(opcode == "b01100" & func3 == 5 & func7[5])
		name = "32sSRA";
	IF(opcode == "b01100" & func3 == 6)
		name = "32sORI";
	IF(opcode == "b01100" & func3 == 7)
		name = "32sAND";
	IF(opcode == "b01100" & rd == 0)
		name = "32sNOOP";

	IF(opcode == "b00011")
		name = "32sFENC";
	IF(opcode == "b11100")
		name = "32sESYS";
}

gtry::scl::riscv::RV32I::RV32I(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	m_area{"rv32i", true},
	m_IP(instructionAddrWidth),
	m_dataAddrWidth(dataAddrWidth)
{
	m_IPnext = m_IP + 4;
	m_area.leave();
}

void gtry::scl::riscv::RV32I::execute()
{
	auto entRV = m_area.enter("execute");

	m_trace.name = m_area.instancePath();
	m_trace.instructionValid = !m_stall & !m_discardResult;
	m_trace.instruction = m_instr.instruction;
	m_trace.instructionPointer = zext(m_IP) | m_IPoffset;
	m_trace.regWriteValid = m_resultValid & !m_stall & m_instr.rd != 0;
	m_trace.regWriteData = m_resultData;
	m_trace.regWriteAddress = m_instr.rd;
	m_trace.memWriteValid = '0';

	HCL_NAMED(m_resultData);
	HCL_NAMED(m_resultValid);
	HCL_NAMED(m_stall);
	m_resultData = 0;
	m_resultValid = '0';
	m_stall = '0';

	selectInstructions();
}

void gtry::scl::riscv::RV32I::selectInstructions()
{
	csr();
	lui();
	auipc();
	jal();
	branch();
	arith();
	logic();
	setcmp();
	shift();
}

void gtry::scl::riscv::RV32I::lui()
{
	IF(m_instr.opcode == "b01101")
		setResult(m_instr.immU);
}

void gtry::scl::riscv::RV32I::auipc()
{
	IF(m_instr.opcode == "b00101")
	{
		auto entAuipc = Area{"auipc"}.enter();
		setResult(m_instr.immU + zext(m_IP) | m_IPoffset);
	}
}

void gtry::scl::riscv::RV32I::jal()
{
	// JAL
	IF(m_instr.opcode == "b11011")
	{
		auto ent = Area{ "jal" }.enter();

		setResult(zext(m_IPnext) | m_IPoffset);
		setIP(zext(m_IP) + m_instr.immJ);
	}

	// JALR
	IF(m_instr.opcode == "b11001")
	{
		auto ent = Area{ "jalr" }.enter();
		m_alu.op2 = m_instr.immI;

		setResult(zext(m_IPnext) | m_IPoffset);
		setIP(m_aluResult.sum);
	}
}

void gtry::scl::riscv::RV32I::branch()
{
	IF(m_instr.opcode == "b11000")
	{
		auto ent = Area{ "branch" }.enter();
		HCL_NAMED(m_IP);
		UInt target = m_IP + m_instr.immB(0, m_IP.width());
		HCL_NAMED(target);
		
		m_alu.sub = '1';

		// equal
		IF(m_instr.func3 == "b000" & m_aluResult.zero)
			setIP(target);

		// not equal
		IF(m_instr.func3 == "b001" & !m_aluResult.zero)
			setIP(target);

		// less than
		IF(m_instr.func3 == "b100" & m_aluResult.sign != m_aluResult.overflow)
			setIP(target);

		// greater than or equal
		IF(m_instr.func3 == "b101" & m_aluResult.sign == m_aluResult.overflow)
			setIP(target);

		// less than unsigned
		IF(m_instr.func3 == "b110" & !m_aluResult.carry)
			setIP(target);

		// greater than or equal unsigned
		IF(m_instr.func3 == "b111" & m_aluResult.carry)
			setIP(target);
	}
}

void gtry::scl::riscv::RV32I::arith()
{
	IF(m_instr.func3 == 0)
	{
		auto ent = Area{ "arith" }.enter();

		IF(m_instr.opcode == "b00100")
		{
			m_alu.op2 = m_instr.immI;
			setResult(m_aluResult.sum);
		}

		IF(m_instr.opcode == "b01100")
		{
			m_alu.sub = m_instr.func7[5];
			setResult(m_aluResult.sum);
		}
	}
}

void gtry::scl::riscv::RV32I::logic()
{
	IF(m_instr.opcode[4] == '0' & m_instr.opcode(0, 3_b) == "b100")
	{
		auto ent = Area{ "logic" }.enter();

		UInt op2 = m_instr.immI;
		IF(m_instr.opcode[3])
			op2 = m_r2;

		IF(m_instr.func3 == 4)
			setResult(m_r1 ^ op2);
		IF(m_instr.func3 == 6)
			setResult(m_r1 | op2);
		IF(m_instr.func3 == 7)
			setResult(m_r1 & op2);
	}
}

void gtry::scl::riscv::RV32I::setcmp()
{
	IF(m_instr.opcode[4] == '0' & m_instr.opcode(0, 3_b) == "b100" & m_instr.func3(1, 2_b) == "b01")
	{
		auto ent = Area{ "setcmp" }.enter();

		IF(m_instr.opcode[3] == '0')
			m_alu.op2 = m_instr.immI;
		m_alu.sub = '1';

		Bit lt = !m_aluResult.carry; // unsigned
		IF(m_instr.func3.lsb() == '0')
			lt = m_aluResult.sign != m_aluResult.overflow;

		setResult(zext(lt));
	}
}

void gtry::scl::riscv::RV32I::shift()
{
	IF(m_instr.opcode[4] == '0' & m_instr.opcode(0, 3_b) == "b100" & m_instr.func3(0, 2_b) == "b01")
	{
		auto ent = Area{ "shift" }.enter();

		UInt amount = m_r2(0, 5_b);
		IF(m_instr.opcode[3] == '0')
			amount = m_instr.immI(0, 5_b);

		UInt number = m_r1;

		Bit left = !m_instr.func3[2];
		IF(left)
			number = swapEndian(number, 1_b);

		Bit arithmetic = m_instr.func7[5];
		number = shr(number, amount, arithmetic);

		IF(left)
			number = swapEndian(number, 1_b);

		setResult(number);
	}
}

void gtry::scl::riscv::RV32I::csr()
{
	auto ent = m_area.enter("csr");

	IF(m_instr.opcode.upper(5_b) == "b11100" & m_instr.func3 != 0)
	{
		setResult(ConstUInt(0, 32_b));
	}
}

void gtry::scl::riscv::RV32I::csrMachineInformation(uint32_t vendorId, uint32_t architectureId, uint32_t implementationId, uint32_t hartId, uint32_t configPtr)
{
	if (vendorId) 
		csrRegister(0xF11, vendorId);
	if (architectureId)
		csrRegister(0xF12, architectureId);
	if (implementationId)
		csrRegister(0xF13, implementationId);
	if (hartId)
		csrRegister(0xF14, hartId);
	if (configPtr)
		csrRegister(0xF15, configPtr);
}

void gtry::scl::riscv::RV32I::csrCycle(BitWidth regW)
{
	auto ent = m_area.enter("csrCycle");

	UInt cycles = regW;
	cycles = reg(cycles + 1, 0);
	HCL_NAMED(cycles);

	csrRegister(0xC00, cycles);
}

void gtry::scl::riscv::RV32I::csrInstructionsRetired(BitWidth regW)
{
	auto ent = m_area.enter("csrInstructionsRetired");

	UInt instructions = regW;

	// we decouple instruction detection and counting for better timing.
	// this will make the instruction counter lag behind 1 cycle.
	IF(reg(!m_discardResult & !m_stall, '0'))
		instructions += 1;

	instructions = reg(instructions, 0);
	HCL_NAMED(instructions);

	csrRegister(0xC02, instructions);
}

void gtry::scl::riscv::RV32I::csrTime(BitWidth regW, hlim::ClockRational resolution)
{
	auto ent = m_area.enter("csrTime");

	auto timerCycles = resolution * ClockScope::getClk().absoluteFrequency();
	scl::Counter timer{ timerCycles.numerator() / timerCycles.denominator() };
	timer.inc();
	Bit tickTimer = reg(timer.isLast(), '0');
	HCL_NAMED(tickTimer);

	UInt timeReg = regW;
	IF(tickTimer)
		timeReg += 1;
	timeReg = reg(timeReg, 0);
	HCL_NAMED(timeReg);

	csrRegister(0xC01, timeReg);
}

void gtry::scl::riscv::RV32I::mem(AvalonMM& mem, bool byte, bool halfword)
{
	auto entRV = m_area.enter();

	mem.read = '0';
	mem.write = '0';
	mem.address = m_aluResult.sum(0, m_dataAddrWidth);
	mem.address(0, 2_b) = 0;
	mem.writeData = m_r2;
	mem.byteEnable = "b1111";

	// check for unaligned access
	Bit is_access = (m_instr.opcode == "b00000" | m_instr.opcode == "b01000") & !m_discardResult;
	UInt access_width = m_instr.func3(0, 2_b);
	sim_assert(!(is_access & access_width == 2) | m_aluResult.sum(0, 2_b) == 0) << "Unaligned access when loading 32 bit word: is_access " << is_access << " access_width: " << access_width << " m_aluResult.sum: " << m_aluResult.sum;
	sim_assert(!(is_access & access_width == 1) | m_aluResult.sum(0, 1_b) == 0) << "Unaligned access when loading 16 bit word: is_access " << is_access << " access_width: " << access_width << " m_aluResult.sum: " << m_aluResult.sum;

	store(mem, byte, halfword);
	load(mem, byte, halfword);
}

gtry::scl::TileLinkUL gtry::scl::riscv::RV32I::memTLink(bool byte, bool halfword)
{
	auto entRV = m_area.enter("mem");
	auto mem = tileLinkInit<TileLinkUL>(32_b, 32_b);

	setFullByteEnableMask(mem.a); // set mask according to size and address
	valid(mem.a) = '0';
	mem.a->opcode = (size_t)TileLinkA::Get;
	mem.a->param = 0;
	mem.a->source = 0;
	mem.a->address = m_aluResult.sum;

	mem.a->data = (BVec)m_r2;
	mem.a->size = 2;
	if (byte || halfword)
	{
		auto tmp = m_instr.func3.lower(2_b);
		setName(tmp, "FIXME");
		mem.a->size = tmp;

		UInt byteVal = m_r2.lower(8_b);
		UInt wordVal = m_r2.lower(16_b);
		if (byte)
			IF(mem.a->size == 0)
				mem.a->data = (BVec)cat(byteVal, byteVal, byteVal, byteVal);
		if (halfword)
			IF(mem.a->size == 1)
				mem.a->data = (BVec)cat(wordVal, wordVal);
	}

	ready(*mem.d) = '1';

	enum class ReqState { req, wait };
	Reg<Enum<ReqState>> state{ ReqState::req };
	state.setName("state");

	Bit issueRequest = '0';

	// load
	IF(m_instr.opcode == "b00000")
	{
		m_alu.op2 = m_instr.immI;
		issueRequest = '1';

		UInt value = (UInt)(*mem.d)->data;
		value |= (*mem.d)->error;
		HCL_NAMED(value);

		if (byte)
		{
			IF(mem.a->size == 0)
			{
				UInt byte = muxWord(mem.a->address.lower(2_b), value);
				IF(m_instr.func3.msb())
					value = zext(byte);
				ELSE
					value = sext(byte);
			}
		}
		if (halfword)
		{
			IF(mem.a->size == 1)
			{
				UInt word = muxWord(mem.a->address[1], value);
				IF(m_instr.func3.msb())
					value = zext(word);
				ELSE
					value = sext(word);
			}
		}

		setResult(value);
	}

	// store
	IF(m_instr.opcode == "b01000")
	{
		issueRequest = '1';
		m_alu.op2 = m_instr.immS;
		mem.a->opcode = (size_t)TileLinkA::PutFullData;
	}

	IF(issueRequest & !m_discardResult)
	{
		IF(state.current() == ReqState::req)
			valid(mem.a) = '1';
		IF(transfer(mem.a))
			state = ReqState::wait;

		Bit done = transfer(*mem.d);
		IF(done)
			state = ReqState::req;
		setStall(!done);
	}

	IF(issueRequest & !m_discardResult)
	{
		// we do not support unaligned access (out of spec)
		sim_assert(mem.a->address.lower(2_b) == 0 | mem.a->size != 2) << __FILE__ << " " << __LINE__;
		sim_assert(mem.a->address.lower(1_b) == 0 | mem.a->size != 1) << __FILE__ << " " << __LINE__;
	}

	setName(mem, "dmem");
	return mem;
}

void gtry::scl::riscv::RV32I::store(AvalonMM& mem, bool byte, bool halfword)
{
	IF(m_instr.opcode == "b01000" & !m_discardResult)
	{
		auto ent = Area{ "store" }.enter();

		m_alu.op2 = m_instr.immS;
		mem.write = '1';

		if (byte)
		{
			IF(m_instr.func3 == 0)
			{
				mem.writeData = cat(m_r2(0, 8_b), m_r2(0, 8_b), m_r2(0, 8_b), m_r2(0, 8_b));
				mem.byteEnable = decoder(m_aluResult.sum(0, 2_b));
			}
		}
		if (halfword)
		{
			IF(m_instr.func3 == 1)
			{
				mem.writeData = cat(m_r2(0, 16_b), m_r2(0, 16_b));

				Bit highWord = m_aluResult.sum[1];
				mem.byteEnable = cat(highWord, highWord, !highWord, !highWord);
			}
		}
		mem.setName("store_");

	}

	m_trace.memWriteValid = *mem.write;
	m_trace.memWriteAddress = mem.address;
	m_trace.memWriteData = *mem.writeData;
	m_trace.memWriteByteEnable = *mem.byteEnable;
}

void gtry::scl::riscv::RV32I::load(AvalonMM& mem, bool byte, bool halfword)
{
	IF(m_instr.opcode == "b00000" & !m_discardResult)
	{
		auto ent = Area{ "load" }.enter();

		m_alu.op2 = m_instr.immI;

		Bit readStallState;
		readStallState = reg(readStallState, '0');
		HCL_NAMED(readStallState);

		IF(!readStallState)
			mem.read = '1';

		IF(*mem.read)
			readStallState = '1';

		mem.createReadDataValid();
		IF(*mem.readDataValid)
			readStallState = '0';
		setStall(!*mem.readDataValid);

		UInt value = *mem.readData;

		// LB, LBU, LH, LHU
		UInt offset = m_aluResult.sum(0, 2_b);
		UInt type = m_instr.func3(0, 2_b);
		Bit zero = m_instr.func3.msb();
		if (byte)
		{
			IF(type == 0) // byte load
			{
				UInt byte = muxWord(offset, value);
				IF(zero)
					value = zext(byte);
				ELSE
					value = sext(byte);

				mem.byteEnable = decoder(m_aluResult.sum(0, 2_b));
			}
		}
		if (halfword)
		{
			IF(type == 1) // word load
			{
				UInt word = muxWord(offset[1], value);
				IF(zero)
					value = zext(word);
				ELSE
					value = sext(word);

				Bit highWord = m_aluResult.sum[1];
				mem.byteEnable = cat(highWord, highWord, !highWord, !highWord);
			}
		}

		IF(*mem.readDataValid)
			setResult(value);
	}
}

void gtry::scl::riscv::RV32I::csrRegister(size_t address, const UInt& data)
{
	IF(m_instr.opcode.upper(5_b) == "b11100" & m_instr.func3 != 0)
	{
		IF(m_instr.immI.lower(12_b) == address)
			setResult(data.width() >= 32_b ? data.lower(32_b) : zext(data));
		if (data.width() > 32_b)
			IF(m_instr.immI.lower(12_b) == (address | 0x80))
				setResult(zext(data(32, data.width() - 32_b)));
	}
}

void gtry::scl::riscv::RV32I::setupAlu()
{
	auto ent = m_area.enter();

	// int alu
	HCL_NAMED(m_alu);
	m_alu.result(m_aluResult);
	HCL_NAMED(m_aluResult);

	m_alu.op1 = m_r1;
	m_alu.op2 = m_r2;
	m_alu.sub = '0';
}

void gtry::scl::riscv::IntAluCtrl::result(IntAluResult& out) const
{
	auto ent = Area{ "IntAlu" }.enter();

	auto [sum, carry] = add(op1, op2 ^ sub, sub);

	out.sum = sum;
	out.carry = carry.msb();

	out.zero = op1 == op2;
	out.sign = sum.msb();

	out.overflow = carry[carry.size() - 2] ^ carry.msb();

	setName(out, "alu_result");
}

gtry::scl::riscv::SingleCycleI::SingleCycleI(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	RV32I(instructionAddrWidth, dataAddrWidth),
	m_resultIP(instructionAddrWidth)
{
	m_discardResult = '0';
}

gtry::Memory<gtry::UInt>& gtry::scl::riscv::SingleCycleI::fetch(uint32_t firstInstructionAddr)
{
	auto entRV = m_area.enter("fetch");

	BitWidth memWidth = m_IP.width() - 2;
	m_instructionMem.setup(memWidth.count(), 32_b);
	m_instructionMem.setType(MemType::SMALL);

	UInt addr = m_IP.width();
	UInt instruction = reg(m_instructionMem[addr(2, memWidth)].read());
	//UInt instruction = m_instructionMem[reg(addr(2, memWidth))];

	Bit firstInstr = reg(Bit{ '0' }, '1');
	HCL_NAMED(firstInstr);
	IF(firstInstr) // noop first instruction
	{
		setStall('1');
		instruction = 0x13;
	}

	HCL_NAMED(instruction);
	addr = fetch(instruction, firstInstructionAddr);

	return m_instructionMem;
}

gtry::UInt gtry::scl::riscv::SingleCycleI::fetch(const UInt& instruction, uint32_t firstInstructionAddr)
{
	m_instr.decode(instruction);
	HCL_NAMED(m_instr);

	IF(!m_stall)
		m_IP = m_resultIP;

	UInt ifetchAddr = m_IP;
	HCL_NAMED(ifetchAddr);

	m_IP = reg(m_IP, firstInstructionAddr);
	HCL_NAMED(m_IP);
	HCL_NAMED(m_resultIP);
	m_resultIP = m_IPnext;
	return ifetchAddr;
}

void gtry::scl::riscv::SingleCycleI::fetchOperands(BitWidth regAddrWidth)
{
	{
		auto entRV = m_area.enter("fetchOperands1");

		m_rf.setup(regAddrWidth.count(), 32_b);
		m_rf.initZero();

		m_r1 = m_rf[m_instr.rs1(0, regAddrWidth)];
		m_r2 = m_rf[m_instr.rs2(0, regAddrWidth)];
		HCL_NAMED(m_r1);
		HCL_NAMED(m_r2);
	}
	setName(m_r1, "r1");
	setName(m_r2, "r2"); // work around for out port used as instance input

	{
		auto entRV = m_area.enter("fetchOperands2");
		setupAlu();

		// this should move into write back (requires write before read policy)
		HCL_NAMED(m_resultData);
		HCL_NAMED(m_resultValid);
		HCL_NAMED(m_stall);
		IF(m_resultValid & !m_stall & m_instr.rd != 0)
			m_rf[m_instr.rd(0, regAddrWidth)] = m_resultData;

		m_resultData = 0;
		m_resultValid = '0';
		m_stall = '0';
	}
}


void gtry::scl::riscv::SingleCycleI::setIP(const UInt& ip)
{
	m_resultIP = ip(0, m_IP.width());
}

void gtry::scl::riscv::RV32I::setResult(const UInt& result)
{
	IF(!m_discardResult)
		m_resultValid = '1';
	m_resultData = zext(result);
}

void gtry::scl::riscv::RV32I::setStall(const Bit& wait)
{
	m_stall |= wait;
}
