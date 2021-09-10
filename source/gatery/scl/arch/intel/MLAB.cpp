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
#include "gatery/pch.h"
#include "MLAB.h"

namespace gtry::scl::arch::intel {

GenericMemoryDesc buildMLABDesc(MLABVariants variant, bool withOutputReg)
{
	GenericMemoryDesc res;

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	const size_t size = 640;

	res.memoryName = "MLAB";
	res.sizeCategory = MemoryCapabilities::SizeCategory::SMALL;

	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	size_t maxDepth = 64;
	if (variant == MLABVariants::ArriaV || variant == MLABVariants::CycloneV)
		maxDepth = 32;
	for (size_t i = 32; i <= 64; i *= 2) {
		res.sizeConfigs.push_back({
			.width = size/i,
			.depth = i,
		});
	}	
	// NOTE: MLABs can't do mixed width ports!

	// See Embedded Memory User Guide "Table 5. Supported Mixed-Width Ratio Configurations for Intel Arria 10"
	// need to check this, note that on arria 10 it changes the possible port aspect ratios.
	// Embedded Memory User Guide "3.9. Byte Enable"
	res.byteEnableByteWidths = {5, 8, 9, 10};
	
	res.numWritePorts = 1;
	res.numReadPorts = 1;
	res.numReadWritePorts = 0;

	res.portsCanDisable = false;
	res.portsMustShareClocks = true;

	if (withOutputReg) {
		res.crossPortReadDuringWrite
			.insert(GenericMemoryDesc::ReadDuringWriteBehavior::MUST_NOT_HAPPEN);
	} else {
		res.crossPortReadDuringWrite
			.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_UNDEFINED);
	}

	if (withOutputReg) {
		res.readLatencies = {0};
	} else {
		res.dataOutputRegisters
			.insert(GenericMemoryDesc::RegisterFlags::EXISTS);
			//.insert(GenericMemoryDesc::RegisterFlags::CAN_RESET) // unsure about these
			//.insert(GenericMemoryDesc::RegisterFlags::CAN_STALL); // unsure about these
		res.readLatencies = {1};
	}

	res.costPerUnitSize = 250; // about 1/4th of a block ram since there are ~ 4x more lutrams
	res.unitSize = size;

	return res;
}


}