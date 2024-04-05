/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include "DebugVis.h"

#include "riscv.h"

#include <gatery/simulation/SimulationVisualization.h>



namespace gtry::scl::riscv 
{


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


struct RFMirror {
	std::array<uint32_t, 32> value;
	std::array<uint32_t, 32> defined;
};

void renderRegisterFileSvg(std::ostream &stream, const RFMirror &rf, 
		const sim::DefaultBitVectorState &write, const sim::DefaultBitVectorState &writeIdx, 
		const sim::DefaultBitVectorState &readIdx_0, const sim::DefaultBitVectorState &readIdx_1)
{
	stream << R"_(
	<svg xmlns='http://www.w3.org/2000/svg' height='280' width='470'>
		<defs>
			<linearGradient id='regFieldGrad' x1='0' y1='0' x2='0' y2='16' gradientUnits='userSpaceOnUse'>
				<stop style='stop-color:#303030;stop-opacity:1' offset='0' id='stop0' />
				<stop style='stop-color:#5d5d5d;stop-opacity:1' offset='0.4' id='stop1' />
				<stop style='stop-color:#dedede;stop-opacity:1' offset='0.5' id='stop2' />
				<stop style='stop-color:#dfdfdf;stop-opacity:1' offset='1' id='stop3' />
			</linearGradient>
			<filter id='reading' x="-50%" y="-20%" width="200%" height="140%">
			  <feDropShadow dx='4' dy='0' stdDeviation='2' flood-color='blue' flood-opacity='0.8'/>
			</filter>			
			<filter id='writing' x="-50%" y="-20%" width="200%" height="140%">
			  <feDropShadow dx='-4' dy='0' stdDeviation='2' flood-color='green' flood-opacity='0.8'/>
			</filter>			
			<filter id='reading_and_writing' x="-50%" y="-20%" width="200%" height="140%">
			  <feDropShadow dx='4' dy='0' stdDeviation='2' flood-color='blue' flood-opacity='0.8'/>
			  <feDropShadow dx='-4' dy='0' stdDeviation='2' flood-color='green' flood-opacity='0.8'/>
			</filter>
		</defs>
	)_";

	bool readingRegs[32] = {};
	bool writingRegs[32] = {};

	auto buildWriteLine = [&](const char *style, size_t regIdx) {
		auto x = regIdx % 8;
		auto y = regIdx / 8;

		float fy = (y * 8 + x) * 8.5f + 8.0f;
		float fx = 10.0f + x * 56.0f;

		stream << boost::format(R"_(
			<polyline style='fill:none;%s' points='0,140 5,140 5,%f %f,%f'/>
		)_") % style % fy % fx % fy;
	};

	for (auto i : utils::Range(1, 32))
		buildWriteLine("stroke:rgb(200,200,200)", i);

	if ((write.get(sim::DefaultConfig::DEFINED) && write.get(sim::DefaultConfig::VALUE)) || !write.get(sim::DefaultConfig::DEFINED)) {
		auto writeIdxValue = writeIdx.head(sim::DefaultConfig::VALUE);
		if (sim::allDefined(writeIdx) && writeIdxValue < 32) {
			writingRegs[writeIdxValue] = true;
			if (write.get(sim::DefaultConfig::DEFINED))
				buildWriteLine("stroke:rgb(0,255,0)", writeIdxValue);
			else
				buildWriteLine("stroke:rgb(255,0,0)", writeIdxValue);
		} else {
			for (auto i : utils::Range(1, 32)) {
				writingRegs[i] = true;
				buildWriteLine("stroke:rgb(255,0,0)", i);
			}
		}
	}

	auto buildReadLine = [&](const char *style, size_t regIdx, size_t dst) {
		auto x = regIdx % 8;
		auto y = regIdx / 8;

		float fy = (y * 8 + x) * 8.5f + 8.0f;
		float fx = 10.0f + x * 56.0f + 48.0f;

		float routingX = 460.0f + dst*5.0f;
		float exitY = 135.0f + dst * 10.0f;

		stream << boost::format(R"_(
			<polyline style='fill:none;%s' points='%f,%f %f,%f %f,%f 470,%f'/>
		)_") % style % fx % fy % routingX % fy % routingX % exitY % exitY;
	};	

	for (auto i : utils::Range(0, 32)) {
		buildReadLine("stroke:rgb(200,200,200)", i, 0);
		buildReadLine("stroke:rgb(200,200,200)", i, 1);
	}

	auto readIdx_0_Value = readIdx_0.head(sim::DefaultConfig::VALUE);
	if (sim::allDefined(readIdx_0) && readIdx_0_Value < 32) {
		readingRegs[readIdx_0_Value] = true;
		buildReadLine("stroke:rgb(0,0,255)", readIdx_0_Value, 0);
	} else {
		for (auto i : utils::Range(0, 32)) {
			readingRegs[i] = true;
			buildReadLine("stroke:rgb(255,0,0)", i, 0);
		}
	}

	auto readIdx_1_Value = readIdx_1.head(sim::DefaultConfig::VALUE);
	if (sim::allDefined(readIdx_1) && readIdx_1_Value < 32) {
		readingRegs[readIdx_1_Value] = true;
		buildReadLine("stroke:rgb(0,0,255)", readIdx_1_Value, 1);
	} else {
		for (auto i : utils::Range(0, 32)) {
			readingRegs[i] = true;
			buildReadLine("stroke:rgb(255,0,0)", i, 1);
		}
	}




	auto buildField = [&](const char *header, const char *footer, float x, float y, bool reading, bool writing) {
		std::string animation = "";
		if (reading && writing)
			animation = "filter:url(#reading_and_writing);";
		else if (reading)
			animation = "filter:url(#reading);";
		else if (writing)
			animation = "filter:url(#writing);";

		stream << boost::format(R"_(
			<g transform='translate(%f,%f)'>
				<rect style='fill:url(#regFieldGrad);%s' stroke='black' stroke-width='0.25' width='48' height='16' x='0' y='0' ry='3'/>
				<text style='font-family:monospace;font-size:4px;fill:#FFFFFF;text-anchor:start' x='3' y='6.5'>%s</text>
				<text style='font-family:monospace;font-weight:bold;font-size:6.5px;fill:#000000;text-anchor:end' x='45' y='15'>%s</text>
			</g>
		)_") % x % y % animation % header % footer;
	};

	for (auto y = 0; y < 4; y++) {
		for (auto x = 0; x < 8; x++) {
			auto regIdx = x + y*8;

			std::stringstream value;
			value << "0x";
			for (auto i : utils::Range(8)) {
				size_t v = (rf.value[regIdx] >> (i*4)) & 0xF;
				size_t d = (rf.defined[regIdx] >> (i*4)) & 0xF;
				if (d != 0xF)
					value << 'X';
				else
					value << std::hex << v;
			}

			buildField(riscV_regNames[regIdx], value.str().c_str(), 10.0f + x * 56.0f, (y * 8.0f + x) * 8.5f, readingRegs[regIdx], writingRegs[regIdx]);
		}
	}



	stream << "</svg>";
}


void debugVisualizeRiscVRegisterFile(Bit writeRf, UInt wrAddr, UInt wrData, UInt rs1, UInt rs2)
{
	tap(writeRf);
	tap(wrAddr);
	tap(wrData);
	tap(rs1);
	tap(rs2);

	auto visId = dbg::createAreaVisualization(500, 380);

	std::stringstream content;
	content << "<div style='margin: 10px;padding: 10px;'>";
	content << "<h2>RISC-V Register file</h2>";
	content << "</div>";
	dbg::updateAreaVisualization(visId, content.str());

//	auto clock = ClockScope::getClk().getClk();

	addSimViz(sim::SimViz<RFMirror>{}
		.onReset([](RFMirror &rfMirror) {
			for (auto &v : rfMirror.value) v = 0;
			for (auto &v : rfMirror.defined) v = 0;	
		})
		.onCapture([=](RFMirror &rfMirror) {
			auto write = simu(writeRf).eval();
			auto addr = simu(wrAddr).eval();
			auto value = simu(wrData).eval();

			if (!write.get(sim::DefaultConfig::DEFINED, 0) || write.get(sim::DefaultConfig::VALUE, 0)) {
				if (!sim::allDefined(addr)) {
					for (auto &v : rfMirror.value) v = 0;
					for (auto &v : rfMirror.defined) v = 0;
				} else {
					size_t idx = addr.head(sim::DefaultConfig::VALUE) % 32;
					if (!write.get(sim::DefaultConfig::DEFINED, 0))
						rfMirror.defined[idx] = 0;
					else {
						rfMirror.value[idx] = (std::uint32_t) value.head(sim::DefaultConfig::VALUE);
						rfMirror.defined[idx] = (std::uint32_t) value.head(sim::DefaultConfig::DEFINED);
					}
				}
			}
		})
		.onRender([=](RFMirror &rfMirror) {
			auto write = simu(writeRf).eval();
			auto addr = simu(wrAddr).eval();

			auto rs1v = simu(rs1).eval();
			auto rs2v = simu(rs2).eval();


			std::stringstream content;
			content << "<div style='margin: 10px;padding: 10px;'>";
			content << "<h2>RISC-V Register file</h2>";
			renderRegisterFileSvg(content, rfMirror,  write, addr, rs1v, rs2v);
			content << "</div>";			
			dbg::updateAreaVisualization(visId, content.str());
		})
	);
}


void debugVisualizeIP(UInt IP)
{
	tap(IP);

	auto visId = dbg::createAreaVisualization(300, 150);

	std::stringstream content;
	content << "<div style='margin: 10px;padding: 10px;'>";
	content << "<h2>Instruction Pointer</h2>";
	content << "<table><th>value</th><th>defined</th></tr>";
	content << "<tr><td>?</td><td>?</td></tr>";
	content << "</table>";
	content << "</div>";
	dbg::updateAreaVisualization(visId, content.str());

	addSimViz(sim::SimViz<void>{}
		.onRender([=]{
			auto ip = simu(IP).eval();

			std::stringstream content;
			content << "<div style='margin: 10px;padding: 10px;'>";
			content << "<h2>Instruction Pointer</h2>";
			content << "<table><tr><th>value</th><th>defined</th></tr>";

			content << "<tr><td>0x" << std::hex << ip.head(sim::DefaultConfig::VALUE)
					<< "</td><td>0x" << std::hex << ip.head(sim::DefaultConfig::DEFINED) << "</td></tr>";

			content << "</table>";
			content << "</div>";			
			dbg::updateAreaVisualization(visId, content.str());
		})
	);
}

void debugVisualizeInstruction(const Instruction &instruction)
{
	tap(instruction.opcode);
	tap(instruction.instruction);
	tap(instruction.func3);
	tap(instruction.rd);

	auto visId = dbg::createAreaVisualization(300, 150);

	std::stringstream content;
	content << "<div style='margin: 10px;padding: 10px;'>";
	content << "<h2>Instruction Decoder</h2>";
	content << "</div>";
	dbg::updateAreaVisualization(visId, content.str());

	addSimViz(sim::SimViz<void>{}.onRender([=, 
						instruction = instruction.instruction, opcode = instruction.opcode, 
						func3 = instruction.func3, func7 = instruction.func7, rd = instruction.rd]() {

		auto instructionVal = simu(instruction).eval();
		auto opcodeVal = simu(opcode).value();
		auto func3Val = simu(func3).value();
		auto func7Val = simu(func7).value();
		auto rdVal = simu(rd).value();

		std::stringstream content;
		content << "<div style='margin: 10px;padding: 10px;'>";
		content << "<h2>Instruction Decoder</h2>";

		if (!sim::allDefined(instructionVal))
			content << "Instruction partially undefined! <br/>";

		content << "Instruction: 0x" << std::hex << std::setw(8) << std::setfill('0') << instructionVal.extractNonStraddling(sim::DefaultConfig::VALUE, 0, instructionVal.size()) << "<br/>";

		switch (opcodeVal) {
			case 0b01101: content << "LUI"; break;
			case 0b00101: content << "AUIP"; break;
			case 0b11011: content << "JAL"; break;
			case 0b11001:
				switch (func3Val) {
					case 0:	content << "JALR"; break;
					default: content << "INVALID INSTRUCTION"; break;
				} break;
			case 0b11000:
				switch (func3Val) {
					case 0: content << "BEQ"; break;
					case 1: content << "BNE"; break;
					case 4: content << "BLT"; break;
					case 5: content << "BGE"; break;
					case 6: content << "BLTU"; break;
					case 7: content << "BGEU"; break;
					default: content << "INVALID INSTRUCTION"; break;
				} break;
			case 0b00000:
				switch (func3Val) {
					case 0: content << "LB"; break;
					case 1: content << "LH"; break;
					case 2: content << "LW"; break;
					case 4: content << "LBU"; break;
					case 5: content << "LHU"; break;
					default: content << "INVALID INSTRUCTION"; break;
				} break;
			case 0b01000:
				switch (func3Val) {
					case 0: content << "SB"; break;
					case 1: content << "SH"; break;
					case 2: content << "SW"; break;
					default: content << "INVALID INSTRUCTION"; break;
				} break;
			case 0b00100: {
				if (rdVal == 0)
					content << "NOP";
				else
					switch (func3Val) {
						case 0: content << "ADDI"; break;
						case 1: content << "SLLI"; break;
						case 2: content << "SLTI"; break;
						case 3: content << "SLTU"; break;
						case 4: content << "XORI"; break;
						case 5:
							if (func7Val & (1 << 5))
								content << "SRAI";
							else
								content << "SRLI";
						break;
						case 6: content << "ORI"; break;
						case 7: content << "ANDI"; break;
						default: content << "INVALID INSTRUCTION"; break;
					} 
			} break;
			case 0b01100: {
				if (rdVal == 0)
					content << "NOP";
				else
					switch (func3Val) {
						case 0:
							if (func7Val & (1 << 5))
								content << "SUB";
							else
								content << "ADD";
						break;
						case 1: content << "SLL"; break;
						case 2: content << "SLT"; break;
						case 3: content << "SLTU"; break;
						case 4: content << "XOR"; break;
						case 5:
							if (func7Val & (1 << 5))
								content << "SRA";
							else
								content << "SRL";
						break;
						case 6: content << "ORI"; break;
						case 7: content << "AND"; break;
						default: content << "INVALID INSTRUCTION"; break;
					}
			} break;
			case 0b00011: content << "FENC"; break;
			case 0b11100: content << "ESYS"; break;
			default: content << "INVALID INSTRUCTION"; break;
		}

		content << "</div>";
		dbg::updateAreaVisualization(visId, content.str());
	}));
}


}
