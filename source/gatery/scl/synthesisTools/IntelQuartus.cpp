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

#include <gatery/hlim/coreNodes/Node_Pin.h>
#include <gatery/hlim/coreNodes/Node_Register.h>

#include <gatery/hlim/Attributes.h>
#include <gatery/hlim/GraphTools.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/export/vhdl/Entity.h>
#include <gatery/export/vhdl/Package.h>

#include <string_view>
#include <boost/range/adaptor/reversed.hpp>

namespace gtry 
{

	std::string registerClockPin(vhdl::AST& ast, hlim::Node_Register* regNode)
	{
		hlim::NodePort regOutput = { .node = regNode, .port = 0ull };

		std::vector<vhdl::BaseGrouping*> regOutputReversePath;
		if (!ast.findLocalDeclaration(regOutput, regOutputReversePath))
		{
			HCL_ASSERT(false);
			return "node|clk";
		}
		
		std::stringstream regOutputIdentifier;
		for (size_t i = regOutputReversePath.size() - 2; i < regOutputReversePath.size(); --i)
			regOutputIdentifier << regOutputReversePath[i]->getInstanceName() << '|';
		regOutputIdentifier << regOutputReversePath.front()->getNamespaceScope().getName(regOutput);

		auto type = regNode->getOutputConnectionType(0);
		if (type.interpretation == hlim::ConnectionType::BITVEC)
			regOutputIdentifier << "[0]";

		regOutputIdentifier << "|clk";
		return regOutputIdentifier.str();
	}

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
	std::string fullPath = (vhdlExport.getDestination() / filename).string();
	std::fstream file(fullPath.c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	writeClockSDC(*vhdlExport.getAST(), file);

	for (auto& pin : vhdlExport.getAST()->getRootEntity()->getIoPins()) 
	{
		std::string_view direction;
		std::vector<hlim::Node_Register*> allRegs;
		if (pin->isInputPin())
		{
			direction = "input";
			allRegs = hlim::findAllOutputRegisters({ .node = pin, .port = 0ull });
		}
		else if (pin->isOutputPin())
		{
			direction = "output";
			allRegs = hlim::findAllInputRegisters({ .node = pin, .port = 0ull });
		}
		else
		{
			continue;
		}

		if (allRegs.empty())
		{
			file << "# no clock found for " << direction << ' ' << pin->getName() << '\n';
			continue;
		}
		hlim::Node_Register* regNode = allRegs.front();
		hlim::Clock* clock = regNode->getClocks().front();

		bool allOnSameClock = std::all_of(allRegs.begin(), allRegs.end(), [=](hlim::Node_Register* regNode) {
			return regNode->getClocks().front() == clock;
		});
		if (!allOnSameClock)
		{
			file << "# multiple clocks found for " << direction << ' ' << pin->getName() << '\n';
			continue;
		}

		float period = clock->getAbsoluteFrequency().denominator() / float(clock->getAbsoluteFrequency().numerator());
		period *= 1e9f;
		file << "set_" << direction << "_delay " << period / 2;
		file << " -clock " << clock->getName();

		file << " [get_ports " << pin->getName();

		if (pin->getConnectionType().interpretation == hlim::ConnectionType::BITVEC)
			file << "\\[*\\]";
		file << "]";

		file << " -reference_pin " << registerClockPin(*vhdlExport.getAST(), regNode) << "\n";
	}
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
# This script is intended for adding the core to an existing project
#     1. Open the quartus tcl console (View->Utility Windows->Tcl Console) 
#     2. If necessary, change the current working directory to project directory ("cd [get_project_directory]")
#     3. Source this script ("source path/to/this/script.tcl"). Use a relative path to this script if you want files to be added with relative paths (recommended).


package require ::quartus::project

set projectDirectory [get_project_directory]
set currentDirectory [pwd]/

if {$currentDirectory != $projectDirectory} {
	puts "The current working directory must be the project directory!"
	puts "Current working directory: $currentDirectory"
	puts "Project directory: $projectDirectory"
} else {
	variable scriptLocation [info script]
	set directory [file dirname $scriptLocation]
	set pathType [file pathtype $directory]

	puts "The path to the script and files seems to be $directory so prepending filenames with this path."

	if {$pathType != "relative"} {
		puts "Warning: The files are prefixed with an absolute path which breaks the project if it is moved around. Source this tcl script with a relative path if you want relative paths in your project file!"
	}

)" << std::endl;

	auto&& sdcFile = vhdlExport.getConstraintsFilename();
	if (!sdcFile.empty())
		file << "    set_global_assignment -name SDC_FILE $directory/" << sdcFile << '\n';

	for (std::filesystem::path& vhdl_file : sourceFiles(vhdlExport, true, false))
	{
		file << "    set_global_assignment -name VHDL_FILE -hdl_version VHDL_2008 $directory/" << vhdl_file.string();
		if (!vhdlExport.getName().empty())
			file << " -library " << vhdlExport.getName();
		file << '\n';
	}

//	vhdlExport.writeXdc("clocks.xdc");

	file << R"(
	export_assignments
}
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

		// TODO: remove and support modelsim as simulator
		writeModelsimScripts(vhdlExport);
	}

	void IntelQuartus::writeModelsimScripts(vhdl::VHDLExport& vhdlExport)
	{
		for (std::filesystem::path& tb : sourceFiles(vhdlExport, false, true))
		{
			const std::string top = tb.stem().string();
			std::string_view library = vhdlExport.getName().empty() ? 
				std::string_view{ "work" } : 
				vhdlExport.getName();

			std::filesystem::path path = vhdlExport.getDestination() / ("modelsim_" + top + ".do");
			std::ofstream file{ path.string().c_str(), std::ofstream::binary };

			for (std::filesystem::path& source : sourceFiles(vhdlExport, true, false))
				file << "vcom -quiet -2008 -createlib -work " << library << " " << source.string() << '\n';
			file << "vcom -quiet -2008 -work " << library << " " << tb.string() << '\n';

			file << "vsim " << library << "." << top << '\n';
			file << "set StdArithNoWarnings 1\n";
			file << "set NumericStdNoWarnings 1\n";
			file << "add wave *\n";
			file << "config wave -signalnamewidth 1\n";
			file << "run -all\n";
		}
	}
}
