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

#include <gatery/frontend/TechnologyCapabilities.h>
#include <gatery/frontend/TechnologyMappingPattern.h>
#include <gatery/utils/BitFlags.h>

#include <gatery/hlim/supportNodes/Node_External.h>

#include <string>
#include <vector>

namespace gtry::scl::arch {

class TargetDevice;

struct GenericMemoryDesc {
	std::string memoryName;

	enum class SizeCategory {
		SMALL,
		MEDIUM,
		LARGE
	};
	SizeCategory sizeCategory;

	struct SizeConfig {
		size_t width;
		size_t depth;
	};

	std::vector<SizeConfig> sizeConfigs;
	std::vector<size_t> mixedWidthRatios;
	std::vector<size_t> byteEnableByteWidths;

	size_t numWritePorts;
	size_t numReadPorts;
	size_t numReadWritePorts;

	bool portsCanDisable;
	bool portsMustShareClocks;

	enum class ReadDuringWriteBehavior : size_t {
		READ_FIRST,
		WRITE_FIRST,
		READ_UNDEFINED,
		WRITE_UNDEFINED,
		ALL_MEMORY_UNDEFINED,
		MUST_NOT_HAPPEN,
	};

	utils::BitFlags<ReadDuringWriteBehavior> samePortReadDuringWrite;
	utils::BitFlags<ReadDuringWriteBehavior> crossPortReadDuringWrite;

	enum class RegisterFlags : size_t {
		EXISTS,
		OPTIONAL,
		CAN_RESET,
		CAN_RESET_NONZERO,
		CAN_STALL
	};
	utils::BitFlags<RegisterFlags> readAddrRegister;
	utils::BitFlags<RegisterFlags> dataOutputRegisters;
	std::vector<size_t> readLatencies;

	size_t costPerUnitSize;
	size_t unitSize;
};


class GenericMemoryCapabilities : public MemoryCapabilities
{
    public:
		GenericMemoryCapabilities(const TargetDevice &targetDevice);
        virtual ~GenericMemoryCapabilities();

		virtual Choice select(const Request &request) const override;
	protected:
		const TargetDevice &m_targetDevice;
};

class GenericMemoryPattern : public TechnologyMappingPattern
{
	public:
		GenericMemoryPattern(const TargetDevice &targetDevice) : m_targetDevice(targetDevice) { }
		virtual ~GenericMemoryPattern() = default;

		virtual bool scopedAttemptApply(hlim::NodeGroup *nodeGroup) const override;
	protected:
		//virtual void buildMemory() = 0;
		const TargetDevice &m_targetDevice;
};


}