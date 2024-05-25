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

#include <map>
#include <string>
#include <optional>

#include <gatery/utils/PropertyTree.h>
#include <gatery/utils/ConfigTree.h>

namespace gtry::hlim {


struct AttribValue {
	std::string type;
	std::string value;
};

typedef std::map<std::string, AttribValue> ResolvedAttributes;
typedef std::map<std::string, AttribValue> VendorSpecificAttributes;

struct Attributes {
	std::map<std::string, VendorSpecificAttributes> userDefinedVendorAttributes;

	void fuseWith(const Attributes &rhs);
	void loadConfig(const utils::ConfigTree& config);
};

struct GroupAttributes : public Attributes {

};


struct SignalAttributes : public Attributes {
	/// Max fanout of this signal before it's driver is duplicated. 0 is don't care.
	std::optional<size_t> maxFanout; 
	/// Whether the signal may be fused away (e.g. signal between regs to shiftreg)
	std::optional<bool> allowFusing;
	/// Do not optimize this signal during synthesis and implementation
	std::optional<bool> dont_touch;

	void fuseWith(const SignalAttributes &rhs);
};

struct RegisterAttributes : public Attributes {
	enum class ResetType {
		SYNCHRONOUS,
		ASYNCHRONOUS,
		NONE
	};

	enum class UsageType {
		DONT_CARE,
		USE,
		DONT_USE
	};

	enum class Active {
		LOW,
		HIGH
	};

	ResetType resetType = ResetType::SYNCHRONOUS;
	ResetType memoryResetType = ResetType::SYNCHRONOUS;
	bool initializeRegs = true;
	bool initializeMemory = true;
	bool synchronizationRegister = false;
	Active resetActive = Active::HIGH;

	UsageType registerResetPinUsage = UsageType::DONT_CARE;
	UsageType registerEnablePinUsage = UsageType::DONT_CARE;

	// Allows the EDA tool to add up to autoPipelineLimit additional registers.
	// You have to take care of handshake logic yourself.
	// Not all tools support this.
	//	- Vivado might not support this on registers with reset and enable
	//  - Vivado does not support registers with fanout > 1
	// All registers with the same autoPipelineGroup will be pipelined together.
	size_t autoPipelineLimit = 0;
	std::string autoPipelineGroup;
};

inline RegisterAttributes::Active operator!(RegisterAttributes::Active v)
{
	if (v == RegisterAttributes::Active::HIGH) 
		return RegisterAttributes::Active::LOW;
	return RegisterAttributes::Active::HIGH;
}

// all user defined attributes ignore type and value and replace $src and $dst in the attrib name with source and destination cells
struct PathAttributes : public Attributes {
	size_t multiCycle = 0; 
	bool falsePath = false;
};

struct MemoryAttributes : public Attributes {
	bool noConflicts = false;
	/// Whether ports of the memory can be retimed arbitrarily wrt. each other without any hazard logic. This is a very dangerous option.
	bool arbitraryPortRetiming = false;
};

}

extern template class std::map<std::string, gtry::hlim::AttribValue>;
extern template class std::map<std::string, gtry::hlim::VendorSpecificAttributes>;
