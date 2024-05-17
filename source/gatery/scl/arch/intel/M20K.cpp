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

#include "M20K.h"

namespace gtry::scl::arch::intel {

M20K::M20K(const IntelDevice &intelDevice) : IntelBlockram(intelDevice)
{
	m_desc.memoryName = "M20K";

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	m_desc.size = 20 << 10;
	
	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	m_desc.addressBits = 14; // 16384
}

M20KStratix10Agilex::M20KStratix10Agilex(const IntelDevice &intelDevice) : M20K(intelDevice)
{
	m_desc.memoryName = "M20K";
	m_desc.addressBits = 11; // 2048
	m_supportsCoherentReadMode = true;
}

#if 0

GenericMemoryDesc buildM20KDesc(M20KVariants variant)
{
	GenericMemoryDesc res;

	// Embedded Memory User Guide "Table 3. Embedded Memory Blocks in Intel FPGA Devices"
	const size_t size = 20 << 10;

	res.memoryName = "M20K";
	res.sizeCategory = MemoryCapabilities::SizeCategory::MEDIUM;

	// Embedded Memory User Guide "Table 6. Valid Range of Maximum Block Depth for Various Embedded Memory Blocks"
	for (size_t i = 512; i <= 16384; i *= 2) {
		res.sizeConfigs.push_back({
			.width = size/i,
			.depth = i,
		});
	}	

	// Embedded Memory User Guide "3.9. Byte Enable"
	res.byteEnableByteWidths = {8, 9};
	// Educated guess, based on Embedded Memory User Guide "3.5. Mixed-width Ratio Configuration" and Intel Agilex Embedded Memory User Guide "Table 47. Device Family Support for Width Ratios"
	res.mixedWidthRatios = {1, 2, 4, 8, 16, 32};

	// See Embedded Memory User Guide "Table 5. Supported Mixed-Width Ratio Configurations for Intel Arria 10"
	switch (variant) {
		case M20KVariants::Arria10_SDP_woBE:
			res.byteEnableByteWidths = {};
		break;
		case M20KVariants::Arria10_TDP_woBE:
			res.byteEnableByteWidths = {};
			res.mixedWidthRatios = {1, 2, 4, 8, 16};
		break;
		case M20KVariants::Arria10_SDP_wBE:
			res.mixedWidthRatios = {1, 2, 4};
		break;
		case M20KVariants::Arria10_TDP_wBE:
			res.mixedWidthRatios = {1, 2};
		break;
	}


	res.numWritePorts = 0;
	res.numReadPorts = 0;
	res.numReadWritePorts = 2;

	res.portsCanDisable = true;
	res.portsMustShareClocks = false;

	res.samePortReadDuringWrite
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_FIRST)
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::WRITE_FIRST)
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_UNDEFINED);
	res.crossPortReadDuringWrite
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_FIRST)
		.insert(GenericMemoryDesc::ReadDuringWriteBehavior::READ_UNDEFINED);

	res.readAddrRegister
		.insert(GenericMemoryDesc::RegisterFlags::EXISTS)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_RESET)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_STALL);
	res.dataOutputRegisters
		.insert(GenericMemoryDesc::RegisterFlags::EXISTS)
		.insert(GenericMemoryDesc::RegisterFlags::OPTIONAL)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_RESET)
		.insert(GenericMemoryDesc::RegisterFlags::CAN_STALL);
	res.readLatencies = {1, 2};

	res.costPerUnitSize = 1000;
	res.unitSize = size;

	return res;
}

#endif

}
