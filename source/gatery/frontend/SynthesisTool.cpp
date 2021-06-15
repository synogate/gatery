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

#include "SynthesisTool.h"

#include "../export/vhdl/VHDLExport.h"
#include "../export/vhdl/Entity.h"
#include "../export/vhdl/Package.h"


#include <fstream>

namespace gtry {


void SynthesisTool::addUserDefinedAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	for (const auto &vendor : m_vendors) {
		auto it = attribs.userDefinedVendorAttributes.find(vendor);
		if (it != attribs.userDefinedVendorAttributes.end()) {
			const auto &userDefinedList = it->second;
			for (const auto &e : userDefinedList)
				resolvedAttribs.insert(e);
		}
	}
}

void SynthesisTool::addUserDefinedAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	for (const auto &vendor : m_vendors) {
		auto it = attribs.userDefinedVendorAttributes.find(vendor);
		if (it != attribs.userDefinedVendorAttributes.end()) {
			const auto &userDefinedList = it->second;
			for (const auto &e : userDefinedList)
				resolvedAttribs.insert(e);
		}
	}
}


DefaultSynthesisTool::DefaultSynthesisTool()
{
	m_vendors = {
		"all",
	};
}

void DefaultSynthesisTool::resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void DefaultSynthesisTool::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void DefaultSynthesisTool::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	file << "# List of source files in dependency order:" << std::endl;

	std::vector<std::filesystem::path> files;

	for (auto&& package : vhdlExport.getAST()->getPackages())
		files.emplace_back(vhdlExport.getAST()->getFilename("", package->getName()));

	for (auto&& entity : vhdlExport.getAST()->getDependencySortedEntities())
		files.emplace_back(vhdlExport.getAST()->getFilename("", entity->getName()));

	for (auto& f : files)
		file << f.string() << '\n';

	vhdlExport.writeXdc("clocks.xdc");

	file << "# List of xdcs:" << std::endl;

	file << "clocks.xdc" << std::endl;
}



}