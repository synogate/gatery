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

#include "FromConfig.h"

#include "intel/IntelDevice.h"
#include "xilinx/XilinxDevice.h"


namespace gtry::scl::arch {


std::unique_ptr<gtry::TargetTechnology> createTargetTechnologyFromConfig(const gtry::utils::ConfigTree &configTree)
{
	auto vendor = configTree["vendor"].as<std::string>();
	if (vendor == "intel") {
		auto intelDevice = std::make_unique<IntelDevice>();
		intelDevice->fromConfig(configTree);
		return intelDevice;
	} else if (vendor == "xilinx") {
		auto xilinxDevice = std::make_unique<XilinxDevice>();
		xilinxDevice->fromConfig(configTree);
		return xilinxDevice;
	} else HCL_DESIGNCHECK_HINT(false, "Unknown target technology vendor: " + vendor);
	
	return {};
}

}
