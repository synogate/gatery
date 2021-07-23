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

#include "IntelQuartus.h"
#include "common.h"

#include <gatery/hlim/Attributes.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/export/vhdl/Entity.h>
#include <gatery/export/vhdl/Package.h>


namespace gtry {

IntelQuartus::IntelQuartus()
{
	m_vendors = {
		"all",
		"intel",
		"intel_quartus"
	};
}

void IntelQuartus::resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	switch (attribs.registerEnablePinUsage) {		
		case hlim::RegisterAttributes::UsageType::USE: {
			resolvedAttribs.insert({"direct_enable", {"boolean", "true"}});
			resolvedAttribs.insert({"syn_direct_enable", {"boolean", "true"}});
		} break;
		case hlim::RegisterAttributes::UsageType::DONT_USE:
			resolvedAttribs.insert({"direct_enable", {"boolean", "false"}});
			resolvedAttribs.insert({"syn_direct_enable", {"boolean", "false"}});
		break;
		default:
		break;
	}
	switch (attribs.registerResetPinUsage) {		
		case hlim::RegisterAttributes::UsageType::USE: {
			resolvedAttribs.insert({"direct_reset", {"boolean", "true"}});
			resolvedAttribs.insert({"syn_direct_reset", {"boolean", "true"}});
		} break;
		case hlim::RegisterAttributes::UsageType::DONT_USE:
			resolvedAttribs.insert({"direct_reset", {"boolean", "true"}});
			resolvedAttribs.insert({"syn_direct_reset", {"boolean", "true"}});
		break;
		default:
		break;
	}
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void IntelQuartus::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	if (attribs.maxFanout) 
		resolvedAttribs.insert({"maxfan", {"integer", std::to_string(*attribs.maxFanout)}});

	if (attribs.crossingClockDomain && *attribs.crossingClockDomain)
		resolvedAttribs.insert({"keep", {"boolean", "true"}});

	if (attribs.allowFusing && !*attribs.allowFusing)
		resolvedAttribs.insert({"keep", {"boolean", "true"}});

	addUserDefinedAttributes(attribs, resolvedAttribs);
}


void IntelQuartus::writeClocksFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	writeClockSDC(*vhdlExport.getAST(), (vhdlExport.getDestination() / filename).string());
}

void IntelQuartus::writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	// todo: write constraints
}

void IntelQuartus::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);
	file << R"(
# Warning: untested!
package require ::quartus::project

)" << std::endl;

	auto&& sdcFile = vhdlExport.getConstraintsFilename();
	if (!sdcFile.empty())
		file << "set_global_assignment -name SDC_FILE " << sdcFile << '\n';

	for (std::filesystem::path& vhdl_file : sourceFiles(vhdlExport, true, false))
	{
		file << "set_global_assignment -name VHDL_FILE ";
		file << vhdl_file.string();
		if (!vhdlExport.getName().empty())
			file << " -library " << vhdlExport.getName();
		file << '\n';
	}

//	vhdlExport.writeXdc("clocks.xdc");

	file << R"(
export_assignments
)";

}

	void IntelQuartus::writeStandAloneProject(vhdl::VHDLExport& vhdlExport, std::string_view filename)
	{
		{
			// because quartus

			std::filesystem::path path = vhdlExport.getDestination() / filename;
			path.replace_extension(".qpf");

			std::fstream file(path.string().c_str(), std::fstream::out);

		}

		std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);

		file << R"(
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files
set_global_assignment -name DEVICE 10AX115N2F40I1SG
set_global_assignment -name FAMILY "Arria 10"
set_global_assignment -name VHDL_INPUT_VERSION VHDL_2008
set_instance_assignment -name VIRTUAL_PIN ON -to *
set_global_assignment -name ALLOW_REGISTER_RETIMING OFF
)";

		auto&& topEntity = vhdlExport.getAST()->getRootEntity()->getName();
		file << "set_global_assignment -name TOP_LEVEL_ENTITY " << topEntity << '\n';

		auto&& sdcFile = vhdlExport.getConstraintsFilename();
		if (!sdcFile.empty())
			file << "set_global_assignment -name SDC_FILE " << sdcFile << '\n';

		auto&& clocksFile = vhdlExport.getClocksFilename();
		if (!clocksFile.empty())
			file << "set_global_assignment -name SDC_FILE " << clocksFile << '\n';

		for (std::filesystem::path& vhdl_file : sourceFiles(vhdlExport, true, false))
		{
			file << "set_global_assignment -name VHDL_FILE ";
			file << vhdl_file.string();
			if (!vhdlExport.getName().empty())
				file << " -library " << vhdlExport.getName();
			file << '\n';
		}
	}

}
