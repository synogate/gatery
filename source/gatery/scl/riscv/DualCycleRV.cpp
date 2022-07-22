#include "gatery/pch.h"
#include "DualCycleRV.h"
#include "DebugVis.h"

using namespace gtry;
using namespace gtry::scl::riscv;


gtry::scl::riscv::DualCycleRV::DualCycleRV(BitWidth instructionAddrWidth, BitWidth dataAddrWidth) :
	RV32I(instructionAddrWidth, dataAddrWidth)
{

}

gtry::Memory<gtry::UInt>& gtry::scl::riscv::DualCycleRV::fetch(uint64_t entryPoint)
{
	UInt addr = m_IP.width();
	UInt instruction = 32_b;
	{
		auto entRV = m_area.enter();

		BitWidth memWidth = m_IP.width() - 2;
		m_instructionMem.setup(memWidth.count(), 32_b);
		m_instructionMem.setType(MemType::MEDIUM, 1);
		m_instructionMem.setName("instruction_memory");

		IF(!m_stall)
			instruction = m_instructionMem[addr(2, memWidth)];
		instruction = reg(instruction, {.allowRetimingBackward=true});

		HCL_NAMED(instruction);
	}
	addr = fetch(instruction, entryPoint);

	return m_instructionMem;
}

void gtry::scl::riscv::DualCycleRV::writeCallReturnTrace(std::string filename)
{
	auto clk = ClockScope::getClk();
	auto opcode = pinOut(m_instr.opcode).setName("profile_opcode");
	auto rd = pinOut(m_instr.rd).setName("profile_rd");
	auto rs1 = pinOut(m_instr.rs1).setName("profile_rs1");
	auto target = pinOut(m_overrideIP).setName("profile_target");
	auto ip = pinOut(m_IP).setName("profile_ip");
	auto valid = pinOut(reg(m_instructionValid)).setName("profile_valid");

	DesignScope::get()->getCircuit().addSimulationProcess([=]()->sim::SimulationProcess {

		size_t cycle = 0;
		std::ofstream f{ filename.c_str(), std::ofstream::binary};
		f << std::hex;

		while (1)
		{
			co_await WaitClk(clk);
			cycle++;

			//f << cycle << ' ' << (size_t)simu(ip) << '\n';
			if ((simu(opcode) & 29) == 25 && simu(valid) != 0)
			{
				size_t rdV = simu(rd);
				size_t rs1V = simu(rs1);

				bool rs1Hit = rs1V == 5 || rs1V == 1;
				bool rdHit = rdV == 5 || rdV == 1;
				// pop
				if (simu(opcode) == 25)
					if (rs1Hit && (rs1V != rdV || !rdHit))
						f << cycle << ' ' << (size_t)simu(ip) << " R " << (size_t)simu(target) << '\n';

				// push
				if (rdHit)
					f << cycle << ' ' << (size_t)simu(ip) << " C " << (size_t)simu(target) << '\n';
			}
		}
	});
}
gtry::UInt gtry::scl::riscv::DualCycleRV::fetch(const UInt& instruction, uint64_t entryPoint)
{
	auto ent = m_area.enter();

	Instruction pre_inst;
	pre_inst.decode(instruction);
	HCL_NAMED(pre_inst);

	{
		auto scope = m_area.enter("register_file");
		HCL_NAMED(m_resultData);
		HCL_NAMED(m_resultValid);
		HCL_NAMED(m_storeResult);
		HCL_NAMED(m_stall);

		// setup register file
		m_rf.setup(32, 32_b);
		m_rf.setType(MemType::MEDIUM);
		m_rf.initZero();
		m_rf.setName("register_file");

		Bit writeRf = m_resultValid & m_storeResult & !m_stall;
		HCL_NAMED(writeRf);
		IF(writeRf)
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

		debugVisualizeRiscVRegisterFile(writeRf, m_instr.rd, m_resultData, pre_inst.rs1, pre_inst.rs2);
	}

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
	UInt instruction_execute = 32_b;
	IF(!m_stall)
		instruction_execute = instruction;

	{
		auto ent = m_area.enter("InstructionDecode");
		instruction_execute = reg(instruction_execute);
		m_instr.decode(instruction_execute);
		debugVisualizeInstruction(m_instr);
		HCL_NAMED(m_instr);
	}

	UInt ip = m_IP.width();
	{
		auto ent = m_area.enter("IP");
		ip = reg(ip, entryPoint);
		HCL_NAMED(ip);

		IF(!m_stall & !initialStall)
		{
			m_IP = ip;
			ip += 4;
		}
		m_IP = reg(m_IP, 0);

		m_overrideIP = m_IP.width();
		IF(!m_stall & m_instructionValid & m_overrideIPValid)
			ip = m_overrideIP;

		debugVisualizeIP(ip);
	}

	setupAlu();
	//writeCallReturnTrace("call_return.trace");

	HCL_NAMED(m_overrideIPValid);
	HCL_NAMED(m_overrideIP);
	m_overrideIPValid = '0';
	m_overrideIP = 0;
	return ip;
}

void gtry::scl::riscv::DualCycleRV::setIP(const UInt& ip)
{
	IF(m_instructionValid)
	{
		m_overrideIPValid = '1';
		m_overrideIP = ip(0, m_IP.width());
	}
}
