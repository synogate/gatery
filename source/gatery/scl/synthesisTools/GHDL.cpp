#include "GHDL.h"
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

#include "GHDL.h"

#include <gatery/hlim/Attributes.h>

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/export/vhdl/Entity.h>
#include <gatery/export/vhdl/Package.h>

#include <gatery/utils/FileSystem.h>


namespace gtry {

GHDL::GHDL()
{
	m_vendors = {
		"all",
		"ghdl",
	};
}

void GHDL::resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	// ghdl doesn't support any
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void GHDL::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	// ghdl doesn't support any
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void GHDL::resolveAttributes(const hlim::MemoryAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	// ghdl doesn't support any
	addUserDefinedAttributes(attribs, resolvedAttribs);
}


void GHDL::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{

}

void GHDL::writeStandAloneProject(vhdl::VHDLExport& vhdlExport, std::string_view filename)
{
	auto fileHandle = vhdlExport.getTestbenchDestination().writeFile(filename);
	auto &file = fileHandle->stream();

	// "-frelaxed" is necessary for the vivado simulation models
	std::string library;
	if (!vhdlExport.getName().empty())
		library = std::string("--work=") + std::string(vhdlExport.getName()) + ' ';

	auto relativePath = std::filesystem::relative(vhdlExport.getDestinationPath(), vhdlExport.getTestbenchDestinationPath());


	for (std::filesystem::path& vhdl_file : sourceFiles(vhdlExport, true, false))
		file << "ghdl -a --std=08 --ieee=synopsys -frelaxed " << library << (relativePath/vhdl_file) << std::endl;;

	for (const auto &e : vhdlExport.getTestbenchRecorder()) {
		for (const auto &name : e->getDependencySortedEntities())
			file << "ghdl -a --std=08 --ieee=synopsys -frelaxed " << library << vhdlExport.getAST()->getFilename(name) << std::endl;

		file << "ghdl -e --std=08 --ieee=synopsys -frelaxed " << library << e->getDependencySortedEntities().back() << std::endl;
		file << "ghdl -r --std=08 -frelaxed -fsynopsys " << library << e->getDependencySortedEntities().back() << " --ieee-asserts=disable --vcd=" << e->getName() << "_signals.vcd --wave=" << e->getName() << "_signals.ghw" << std::endl;
	}
}

}
