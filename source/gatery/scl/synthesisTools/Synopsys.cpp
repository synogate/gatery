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

#include "Synopsys.h"

#include <gatery/hlim/Attributes.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry {

Synopsys::Synopsys()
{
	m_vendors = {
		"all",
		"synopsys",
	};
}

void Synopsys::resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	switch (attribs.registerEnablePinUsage) {		
		case hlim::RegisterAttributes::UsageType::USE: {
			resolvedAttribs.insert({"syn_direct_enable", {"boolean", "true"}});
			resolvedAttribs.insert({"syn_useenables", {"boolean", "true"}});
		} break;
		case hlim::RegisterAttributes::UsageType::DONT_USE:
			resolvedAttribs.insert({"syn_direct_enable", {"boolean", "false"}});
			resolvedAttribs.insert({"syn_useenables", {"boolean", "false"}});
		break;
		default:
		break;
	}
	switch (attribs.registerResetPinUsage) {		
		case hlim::RegisterAttributes::UsageType::USE: {
//			HCL_DESIGNCHECK_HINT(false, "registerResetPinUsage handling not implemented for synopsys");
		} break;
		case hlim::RegisterAttributes::UsageType::DONT_USE:
//			HCL_DESIGNCHECK_HINT(false, "registerResetPinUsage handling not implemented for synopsys");
		break;
		default:
		break;
	}
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void Synopsys::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	if (attribs.maxFanout != 0) 
		resolvedAttribs.insert({"syn_maxfan", {"integer", std::to_string(attribs.maxFanout)}});

	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void Synopsys::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{
	HCL_ASSERT_HINT(false, "Not implemented yet!");
}

}