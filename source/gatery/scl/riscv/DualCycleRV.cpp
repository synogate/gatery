#include "gatery/pch.h"
#include "DualCycleRV.h"

using namespace gtry;
using namespace gtry::scl::riscv;


gtry::scl::riscv::DualCycleRV::DualCycleRV(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	RV32I(instructionAddrWidth, dataAddrWidth)
{

}

gtry::Memory<gtry::BVec>& gtry::scl::riscv::DualCycleRV::fetch(uint64_t entryPoint)
{
	BVec addr = m_IP.getWidth();
	BVec instruction = 32_b;
	{
		auto entRV = m_area.enter();

		BitWidth memWidth = m_IP.getWidth() - 2;
		m_instructionMem.setup(memWidth.count(), 32_b);
		m_instructionMem.setType(MemType::BRAM);
		m_instructionMem.setName("instruction_memory");

		IF(!m_stall)
			instruction = m_instructionMem[addr(2, memWidth)];
		instruction = reg(instruction, {.allowRetimingBackward=true});

		HCL_NAMED(instruction);
	}
	addr = fetch(instruction, entryPoint);

	return m_instructionMem;
}

gtry::BVec gtry::scl::riscv::DualCycleRV::fetch(const BVec& instruction, uint64_t entryPoint)
{
	Instruction pre_inst;
	pre_inst.decode(instruction);
	HCL_NAMED(pre_inst);

	// setup register file
	m_rf.setup(32, 32_b);
	m_rf.setType(MemType::BRAM);
	m_rf.setPowerOnStateZero();
	m_rf.setName("register_file");

	IF(m_resultValid & m_storeResult & !m_stall)
		m_rf[m_instr.rd] = m_resultData;

	IF(!m_stall)
	{
		m_r1 = m_rf[pre_inst.rs1];
		m_r2 = m_rf[pre_inst.rs2];
	}
	m_r1 = reg(m_r1, {.allowRetimingBackward=true});
	m_r2 = reg(m_r2, {.allowRetimingBackward=true});
	HCL_NAMED(m_r1);
	HCL_NAMED(m_r2);

	// setup state for execute cycle
	IF(!m_stall)
	{
		m_instructionValid = '1';
		m_storeResult = instruction(7, 5_b) != 0;

		IF(m_overrideIPValid)
		{
			m_instructionValid = '0';
			m_storeResult = '0';
		}
	}

	Bit initialStall = reg(Bit{ '0' }, '1');
	IF(initialStall)
	{
		m_instructionValid = '0';
		m_storeResult = '0';
	}

	m_storeResult = reg(m_storeResult, '0');
	m_instructionValid = reg(m_instructionValid, '0');
	HCL_NAMED(m_instructionValid);
	HCL_NAMED(m_storeResult);

	// decode instruction in execute cycle
	BVec instruction_execute = 32_b;
	IF(!m_stall)
		instruction_execute = instruction;
	instruction_execute = reg(instruction_execute);
	m_instr.decode(instruction_execute);
	HCL_NAMED(m_instr);

	BVec ip = m_IP.getWidth();
	ip = reg(ip, entryPoint);
	HCL_NAMED(ip);

	IF(!m_stall & !initialStall)
	{
		m_IP = ip;
		ip += 4;
	}
	m_IP = reg(m_IP, 0);

	m_overrideIP = ip.getWidth();
	IF(!m_stall & m_instructionValid & m_overrideIPValid)
		ip = m_overrideIP;

	setupAlu();

	HCL_NAMED(m_resultData);
	HCL_NAMED(m_resultValid);
	HCL_NAMED(m_stall);
	m_resultData = 0;
	m_resultValid = '0';

	m_stall = '0';


	HCL_NAMED(m_overrideIPValid);
	HCL_NAMED(m_overrideIP);
	m_overrideIPValid = '0';
	m_overrideIP = 0;
	return ip;
}

void gtry::scl::riscv::DualCycleRV::setIP(const BVec& ip)
{
	IF(m_instructionValid)
	{
		m_overrideIPValid = '1';
		m_overrideIP = ip(0, m_IP.getWidth());
	}
}

void gtry::scl::riscv::DualCycleRV::setResult(const BVec& result)
{
	m_resultValid = '1';
	m_resultData = zext(result);
}

void gtry::scl::riscv::DualCycleRV::setStall(const Bit& wait)
{
	m_stall |= wait;
}
