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

#include "Attributes.h"

#include "../utils/ConfigTree.h"

template class std::map<std::string, gtry::hlim::AttribValue>;
template class std::map<std::string, gtry::hlim::VendorSpecificAttributes>;

namespace gtry::hlim {

void Attributes::fuseWith(const Attributes &rhs)
{
	for (const auto &v : rhs.userDefinedVendorAttributes)
		for (const auto &a : v.second)
			userDefinedVendorAttributes[v.first][a.first] = a.second;
}

void Attributes::loadConfig(const utils::ConfigTree& config)
{
#ifdef USE_YAMLCPP
	for (auto it = config.mapBegin(); it != config.mapEnd(); ++it)
	{
		auto&& configValue = *it;
		AttribValue value;
		if (configValue.isScalar())
		{
			value.type = "string";
			value.value = "\"" + configValue.as<std::string>("") + "\"";
		}
		else
		{
			value.type = configValue["type"].as<std::string>("string");
			value.value = configValue["value"].as<std::string>("true");
		}

		VendorSpecificAttributes& attr = userDefinedVendorAttributes["all"];
		attr[it.key()] = value;
	}
#else
	#pragma message ("Loading attributes from ConfigTree is disabled for non yaml-cpp builds!")
#endif
}
	
void SignalAttributes::fuseWith(const SignalAttributes &rhs)
{
	Attributes::fuseWith(rhs);

	if (rhs.maxFanout)
		maxFanout = rhs.maxFanout;
	if (rhs.allowFusing)
		allowFusing = rhs.allowFusing;
}

}
