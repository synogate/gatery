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


namespace gtry::hlim {


struct AttribValue {
	std::string type;
	std::string value;
};

typedef std::map<std::string, AttribValue> ResolvedAttributes;
typedef std::map<std::string, AttribValue> VendorSpecificAttributes;

struct Attributes {
	std::map<std::string, VendorSpecificAttributes> userDefinedVendorAttributes;
};


struct SignalAttributes : public Attributes {
	/// Max fanout of this signal before it's driver is duplicated. 0 is don't care.
	size_t maxFanout = 0; 
	/// Signal crosses a clock domain
	bool crossingClockDomain = false;
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

	ResetType resetType = ResetType::SYNCHRONOUS;
	bool initializeRegs = true;
	bool resetHighActive = true;

	UsageType registerResetPinUsage = UsageType::DONT_CARE;
	UsageType registerEnablePinUsage = UsageType::DONT_CARE;
};

}