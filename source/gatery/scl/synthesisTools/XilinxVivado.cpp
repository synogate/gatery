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

#include "XilinxVivado.h"

#include <gatery/hlim/Attributes.h>
#include <gatery/hlim/supportNodes/Node_PathAttributes.h>

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/export/vhdl/Entity.h>
#include <gatery/export/vhdl/Package.h>


namespace gtry {

XilinxVivado::XilinxVivado()
{
	m_vendors = {
		"all",
		"xilinx",
		"xilinx_vivado",
		"xilinx_vivado_2019.2",
	};
}

void XilinxVivado::resolveAttributes(const hlim::RegisterAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	switch (attribs.registerEnablePinUsage) {		
		case hlim::RegisterAttributes::UsageType::USE: {
			resolvedAttribs.insert({"extract_enable", {"string", "\"yes\""}});
		} break;
		case hlim::RegisterAttributes::UsageType::DONT_USE:
			resolvedAttribs.insert({"extract_enable", {"string", "\"no\""}});
		break;
		default:
		break;
	}
	switch (attribs.registerResetPinUsage) {		
		case hlim::RegisterAttributes::UsageType::USE: {
			resolvedAttribs.insert({"extract_reset", {"string", "\"yes\""}});
		} break;
		case hlim::RegisterAttributes::UsageType::DONT_USE:
			resolvedAttribs.insert({"extract_reset", {"string", "\"no\""}});
		break;
		default:
		break;
	}
	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void XilinxVivado::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	if (attribs.maxFanout != 0) 
		resolvedAttribs.insert({"max_fanout", {"integer", std::to_string(attribs.maxFanout)}});

	if (attribs.crossingClockDomain) {
		resolvedAttribs.insert({"ASYNC_REG", {"string", "\"true\""}});
		resolvedAttribs.insert({"DONT_TOUCH", {"string", "\"true\""}});
	}

	if (!attribs.allowFusing) {
		resolvedAttribs.insert({"SHREG_EXTRACT", {"string", "\"no\""}});
		resolvedAttribs.insert({"DONT_TOUCH", {"string", "\"true\""}});
	}

	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void XilinxVivado::writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	forEachPathAttribute(vhdlExport, circuit, [&](hlim::Node_PathAttributes* pa, std::string start, std::string end) {
		const auto &startConType = hlim::getOutputConnectionType(pa->getDriver(0));
		const auto &endConType = hlim::getOutputConnectionType(pa->getDriver(1));


		if (startConType.interpretation == hlim::ConnectionType::BITVEC)
			file 
				<< "# get net of start signal, must be KEEP\n"
				<< "set net_start [get_nets " << start << "[*]]\n";
		else
			file 
				<< "# get net of start signal, must be KEEP\n"
				<< "set net_start [get_nets " << start << "]\n";

		file << R"(# get driver pin(s)
set pin_start [get_pin -of_object $net_start -filter {DIRECTION == OUT} ]

# get driver(s)
set cell_start [get_cells -of_object $pin_start]
)";


		if (endConType.interpretation == hlim::ConnectionType::BITVEC)
			file 
				<< "# get net of end signal, must be KEEP\n"
				<< "set net_end [get_nets " << end << "[*]]\n";
		else
			file 
				<< "# get net of end signal, must be KEEP\n"
				<< "set net_end [get_nets " << end << "]\n";

		file << R"(# get driver pin(s)
set pin_end [get_pin -of_object $net_end -filter {DIRECTION == OUT} ]

# get driver(s)
set cell_end [get_cells -of_object $pin_end]
)";

		const auto &attribs = pa->getAttribs();
		
		if (attribs.falsePath)
			file 
				<< "# set false path\n"
				<< "set_false_path -from $cell_start -to $cell_end";

		HCL_ASSERT_HINT(attribs.multiCycle == 0, "Not implemented yet!");

		writeUserDefinedPathAttributes(file, attribs, "$cell_start", "$cell_end");
	});

}

void XilinxVivado::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{
	std::fstream file((vhdlExport.getDestination() / filename).string().c_str(), std::fstream::out);
	file.exceptions(std::fstream::failbit | std::fstream::badbit);

	std::vector<std::filesystem::path> files;

	for (auto&& package : vhdlExport.getAST()->getPackages())
		files.emplace_back(vhdlExport.getAST()->getFilename("", package->getName()));

	for (auto&& entity : vhdlExport.getAST()->getDependencySortedEntities())
		files.emplace_back(vhdlExport.getAST()->getFilename("", entity->getName()));

	for (auto& f : files)
	{
		file << "read_vhdl -vhdl2008 ";
		if (!vhdlExport.getName().empty())
			file << "-library " << vhdlExport.getName() << ' ';
		file << f.string() << '\n';
	}

	vhdlExport.writeXdc("clocks.xdc");

	file << R"(
read_xdc clocks.xdc

# reset_run synth_1
# launch_runs impl_1

# set run settings -> more options to "-mode out_of_context" for virtual pins

)";
}

}