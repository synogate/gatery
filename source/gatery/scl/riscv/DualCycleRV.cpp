#include "gatery/pch.h"
#include "DualCycleRV.h"

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
		m_instructionMem.setType(MemType::MEDIUM);
		m_instructionMem.setName("instruction_memory");

		IF(!m_stall)
			instruction = m_instructionMem[addr(2, memWidth)];
		instruction = reg(instruction, {.allowRetimingBackward=true});

		HCL_NAMED(instruction);
	}
	addr = fetch(instruction, entryPoint);

	return m_instructionMem;
}

static const char *riscV_regNames[] = {
	"x0/zero",
	"x1/ra", "x2/sp", "x3/gp", "x4/tp", 
	"x5/t0", "x6/t1", "x7/t2", 
	"x8/s0/fp", "x9/s1",
	"x10/a0", "x11/a1",
	"x12/a2", "x13/a3", "x14/a4", "x15/a5", "x16/a6", "x17/a7",
	"x18/s2", "x19/s3", "x20/s4", "x21/s5", "x22/s6", "x23/s7", "x24/s8", "x25/s9", "x26/s10", "x27/s11",
	"x28/t3", "x29/t4", "x30/t5", "x31/t6"
};


void debugVisualizeRiscVRegisterFile(Bit writeRf, UInt wrAddr, UInt wrData)
{
	sim_tap(writeRf);
	sim_tap(wrAddr);
	sim_tap(wrData);

	auto visId = dbg::createAreaVisualization(300, 800);

	std::stringstream content;
	content << "<div style='margin: 10px;padding: 10px;'>";
	content << "<h2>RISC-V Register file</h2>";
	content << "<table><tr><th>register</th><th>value</th><th>defined</th></tr>";

	for (size_t i = 1; i < 32; i++)
		content << "<tr><td>" << riscV_regNames[i] << "</td><td>?</td><td>?</td></tr>";

	content << "</table>";
	content << "</div>";
	dbg::updateAreaVisualization(visId, content.str());

#if 1
	auto clock = ClockScope::getClk().getClk();
	DesignScope::get()->getCircuit().addSimulationProcess([=]()->sim::SimulationProcess {
		std::vector<std::pair<uint32_t, uint32_t>> rfMirror(32);
		for (auto &p : rfMirror)
			p.first = p.second = 0;

		while (true) {
			auto write = simu(writeRf).eval();
			auto addr = simu(wrAddr).eval();
			auto value = simu(wrData).eval();

			if (!write.get(sim::DefaultConfig::DEFINED, 0) || write.get(sim::DefaultConfig::VALUE, 0)) {
				if (!sim::allDefined(addr)) {
					for (auto &p : rfMirror)
						p.second = 0;
				} else {
					size_t idx = addr.extractNonStraddling(sim::DefaultConfig::VALUE, 0, addr.size()) % 32;
					if (!write.get(sim::DefaultConfig::DEFINED, 0))
						rfMirror[idx].second = 0;
					else {
						rfMirror[idx].first =  value.extractNonStraddling(sim::DefaultConfig::VALUE, 0, value.size());
						rfMirror[idx].second =  value.extractNonStraddling(sim::DefaultConfig::DEFINED, 0, value.size());
					}
				}
			}

			std::stringstream content;
			content << "<div style='margin: 10px;padding: 10px;'>";
			content << "<h2>RISC-V Register file</h2>";
			content << "<table><tr><th>register</th><th>value</th><th>defined</th></tr>";

			for (size_t i = 1; i < 32; i++)
				content << "<tr><td>" << riscV_regNames[i] << "</td><td>0x" << std::hex << rfMirror[i].first << "</td><td>0x" << std::hex << rfMirror[i].second << "</td></tr>";

			content << "</table>";
			content << "</div>";			
			dbg::updateAreaVisualization(visId, content.str());

			co_await WaitClk(clock);
		}
	});
#endif
}


void debugVisualizeIP(UInt IP)
{
	sim_tap(IP);

	auto visId = dbg::createAreaVisualization(300, 150);

	std::stringstream content;
	content << "<div style='margin: 10px;padding: 10px;'>";
	content << "<h2>Instruction Pointer</h2>";
	content << "<table><th>value</th><th>defined</th></tr>";
	content << "<tr><td>?</td><td>?</td></tr>";
	content << "</table>";
	content << "</div>";
	dbg::updateAreaVisualization(visId, content.str());

#if 1
	auto clock = ClockScope::getClk().getClk();
	DesignScope::get()->getCircuit().addSimulationProcess([=]()->sim::SimulationProcess {
		while (true) {
			auto ip = simu(IP).eval();

			std::stringstream content;
			content << "<div style='margin: 10px;padding: 10px;'>";
			content << "<h2>Instruction Pointer</h2>";
			content << "<table><tr><th>value</th><th>defined</th></tr>";

			content << "<tr><td>0x" << std::hex << ip.extractNonStraddling(sim::DefaultConfig::VALUE, 0, ip.size())
					<< "</td><td>0x" << std::hex << ip.extractNonStraddling(sim::DefaultConfig::DEFINED, 0, ip.size()) << "</td></tr>";

			content << "</table>";
			content << "</div>";			
			dbg::updateAreaVisualization(visId, content.str());

			co_await WaitClk(clock);
		}
	});
#endif
}


gtry::UInt gtry::scl::riscv::DualCycleRV::fetch(const UInt& instruction, uint64_t entryPoint)
{
	Instruction pre_inst;
	pre_inst.decode(instruction);
	HCL_NAMED(pre_inst);

	{
		Area area("register_file", true);


		// setup register file
		m_rf.setup(32, 32_b);
		m_rf.setType(MemType::MEDIUM);
		m_rf.initZero();
		m_rf.setName("register_file");

		Bit writeRf = m_resultValid & m_storeResult & !m_stall;
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

		debugVisualizeRiscVRegisterFile(writeRf, m_instr.rd, m_resultData);
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
		Area area("InstructionDecode", true);
		instruction_execute = reg(instruction_execute);
		m_instr.decode(instruction_execute);
		m_instr.createDebugVisualization();
		HCL_NAMED(m_instr);
	}

	UInt ip = m_IP.width();
	{
		Area area("IP", true);
		ip = reg(ip, entryPoint);
		HCL_NAMED(ip);

		IF(!m_stall & !initialStall)
		{
			m_IP = ip;
			ip += 4;
		}
		m_IP = reg(m_IP, 0);

		m_overrideIP = ip.width();
		IF(!m_stall & m_instructionValid & m_overrideIPValid)
			ip = m_overrideIP;

		debugVisualizeIP(ip);
	}

	setupAlu();

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
