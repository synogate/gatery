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
#include "CpuTrace.h"

#include "external/riscv-disas.h"

#include <gatery/simulation/waveformFormats/VCDSink.h>

void gtry::scl::riscv::CpuTrace::pinOut() const
{
	gtry::pinOut(instructionValid).setName(name + "_instructionValid");
	gtry::pinOut(instruction).setName(name + "_instruction");
	gtry::pinOut(instructionPointer).setName(name + "_instructionPointer");
	gtry::pinOut(memWriteValid).setName(name + "_memWriteValid");
	gtry::pinOut(memWriteAddress).setName(name + "_memWriteAddress");
	gtry::pinOut(memWriteData).setName(name + "_memWriteData");
	gtry::pinOut(memWriteByteEnable).setName(name + "_memWriteByteEnable");
	gtry::pinOut(regWriteValid).setName(name + "_regWriteValid");
	gtry::pinOut(regWriteAddress).setName(name + "_regWriteAddress");
	gtry::pinOut(regWriteData).setName(name + "_regWriteData");
}

void gtry::scl::riscv::CpuTrace::writeVcd() const
{
	pinOut();

	const Clock& clk = ClockScope::getClk();

	std::string orgname = name;
	std::string filename = name;
	std::replace(filename.begin(), filename.end(), '/', '_');

	DesignScope::get()->getCircuit().addSimulationProcess([=, trace = *this]()->SimProcess {

		sim::VCDWriter vcd(filename + "_trace.vcd");
		size_t subCylce = 0;
		const size_t decodedLen = 20;

		{
			auto mod = vcd.beginModule(orgname);
			vcd.declareWire(1, "a", "instructionValid");
			vcd.declareWire(trace.instruction.size(), "b", "instruction");
			vcd.declareWire(decodedLen * 8, "B", "instruction_decoded");
			vcd.declareWire(trace.instructionPointer.size(), "c", "instructionPointer");
			vcd.declareWire(1, "d", "memWriteValid");
			vcd.declareWire(trace.memWriteAddress.size(), "e", "memWriteAddress");
			vcd.declareWire(trace.memWriteData.size(), "f", "memWriteData");
			vcd.declareWire(trace.memWriteByteEnable.size(), "g", "memWriteByteEnable");
			vcd.declareWire(1, "h", "regWriteValid");
			vcd.declareWire(trace.regWriteAddress.size(), "i", "regWriteAddress");
			vcd.declareWire(trace.regWriteData.size(), "j", "regWriteData");
			vcd.declareWire(1, "!", "clock");
		}
		vcd.beginDumpVars();

		bool instructionActive = true;
		bool memWriteActive = true;
		bool regWriteActive = true;

		while(1)
		{
			vcd.writeTime(subCylce++);
			vcd.writeBitState("!", true, true);

			if(simu(trace.instructionValid))
			{
				vcd.writeBitState("a", true, true);
				vcd.writeState("b", simu(trace.instruction).eval(), 0, trace.instruction.size());
				vcd.writeState("c", simu(trace.instructionPointer).eval(), 0, trace.instructionPointer.size());

				// the resulting instruction seems to be wrong :/
				std::string decoded;
				disasm_inst(decoded, rv32, simu(trace.instructionPointer), simu(trace.instruction));
				
				size_t src;
				size_t dst = 0;
				for(src = decoded.find_first_not_of(" .\t", 8); src < decoded.size() && decoded[src] != '#'; ++src)
					if(!dst || decoded[src] != ' ' || decoded[dst-1] != ' ' )
						decoded[dst++] = decoded[src];
				decoded.resize(dst);

				vcd.writeString("B", decodedLen * 8, decoded);

				instructionActive = true;
			}
			else if(instructionActive)
			{
				vcd.writeBitState("a", true, false);
				vcd.writeState("b", trace.instruction.size(), 0, 0);
				vcd.writeState("c", trace.instructionPointer.size(), 0, 0);
				instructionActive = false;
			}

			if(simu(trace.memWriteValid))
			{
				vcd.writeBitState("d", true, true);
				vcd.writeState("e", simu(trace.memWriteAddress).eval(), 0, trace.memWriteAddress.size());
				vcd.writeState("f", simu(trace.memWriteData).eval(), 0, trace.memWriteData.size());
				vcd.writeState("g", simu(trace.memWriteByteEnable).eval(), 0, trace.memWriteByteEnable.size());
				memWriteActive = true;
			}
			else if(memWriteActive)
			{
				vcd.writeBitState("d", true, false);
				vcd.writeState("e", trace.memWriteAddress.size(), 0, 0);
				vcd.writeState("f", trace.memWriteData.size(), 0, 0);
				vcd.writeState("g", trace.memWriteByteEnable.size(), 0, 0);
				memWriteActive = false;
			}

			if(simu(trace.regWriteValid))
			{
				vcd.writeBitState("h", true, true);
				vcd.writeState("i", simu(trace.regWriteAddress).eval(), 0, trace.regWriteAddress.size());
				vcd.writeState("j", simu(trace.regWriteData).eval(), 0, trace.regWriteData.size());
				regWriteActive = true;
			}
			else if(regWriteActive)
			{
				vcd.writeBitState("h", true, false);
				vcd.writeState("i", trace.regWriteAddress.size(), 0, 0);
				vcd.writeState("j", trace.regWriteData.size(), 0, 0);
				regWriteActive = false;
			}

			co_await WaitFor(hlim::ClockRational{ 2, 1 } / clk.absoluteFrequency());
			vcd.writeTime(subCylce++);
			vcd.writeBitState("!", true, false);
			co_await AfterClk(clk);

		}
	});




}

