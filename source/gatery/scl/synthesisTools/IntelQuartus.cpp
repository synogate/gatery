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

#include <gatery/frontend/DesignScope.h>
#include "../arch/general/FPGADevice.h"

#include <gatery/hlim/Clock.h>
#include <gatery/hlim/coreNodes/Node_Pin.h>
#include <gatery/hlim/coreNodes/Node_Register.h>
#include <gatery/hlim/coreNodes/Node_Signal.h>
#include <gatery/hlim/supportNodes/Node_CDC.h>

#include <gatery/hlim/Attributes.h>
#include <gatery/hlim/GraphTools.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <gatery/utils/FileSystem.h>

#include <gatery/debug/DebugInterface.h>
#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/export/vhdl/Entity.h>
#include <gatery/export/vhdl/Package.h>

#include <gatery/scl/arch/general/FPGADevice.h>

#include <string_view>
#include <boost/range/adaptor/reversed.hpp>

namespace gtry 
{

	bool registerClockPin(vhdl::AST& ast, hlim::Node_Register* regNode, std::string &path)
	{
		hlim::NodePort regOutput = { .node = regNode, .port = 0ull };

		std::vector<vhdl::BaseGrouping*> regOutputReversePath;
		if (!ast.findLocalDeclaration(regOutput, regOutputReversePath))
			return false;
		
		std::stringstream regOutputIdentifier;
		for (size_t i = regOutputReversePath.size() - 2; i < regOutputReversePath.size(); --i)
			regOutputIdentifier << regOutputReversePath[i]->getInstanceName() << '|';
		regOutputIdentifier << regOutputReversePath.front()->getNamespaceScope().get(regOutput).name;

		auto type = regNode->getOutputConnectionType(0);
		if (type.isBitVec())
			regOutputIdentifier << "[0]";

		regOutputIdentifier << "|clk";
		path = regOutputIdentifier.str();
		return true;
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
	if (attribs.synchronizationRegister) {
		// Quartus 20 Lite complains: Warning (10335): Unrecognized synthesis attribute "adv_netlist_opt_allowed" at dramTesterCyc1000.vhd(6857)
		//resolvedAttribs.insert({"adv_netlist_opt_allowed", {"boolean", "false"}});
		//resolvedAttribs.insert({"direct_reset", {"boolean", "true"}});
		//resolvedAttribs.insert({"syn_direct_reset", {"boolean", "true"}});

		resolvedAttribs.insert({"direct_enable", {"boolean", "true"}});
		resolvedAttribs.insert({"syn_direct_enable", {"boolean", "true"}});
	}

	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void IntelQuartus::resolveAttributes(const hlim::SignalAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	if (attribs.maxFanout) 
		resolvedAttribs.insert({"maxfan", {"integer", std::to_string(*attribs.maxFanout)}});

	if (attribs.allowFusing && !*attribs.allowFusing)
		resolvedAttribs.insert({"adv_netlist_opt_allowed", {"boolean", "false"}});

	addUserDefinedAttributes(attribs, resolvedAttribs);
}

void IntelQuartus::resolveAttributes(const hlim::MemoryAttributes &attribs, hlim::ResolvedAttributes &resolvedAttribs)
{
	/*
	* Too dangerous, because it may brake with LUTRAMs
	if (attribs.noConflicts) 
		resolvedAttribs.insert({"ramstyle", {"string", "\"no_rw_check\""}});
	*/

	addUserDefinedAttributes(attribs, resolvedAttribs);
}


void IntelQuartus::writeClocksFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	auto fileHandle = vhdlExport.getDestination().writeFile(filename);
	auto &file = fileHandle->stream();

	//write clock constraint for altera JTAG
	file << "create_clock -period 100.000 [get_ports altera_reserved_tck]\n";

	writeClockSDC(*vhdlExport.getAST(), file);

	auto* fpga = dynamic_cast<scl::arch::FPGADevice*>(DesignScope::get()->getTargetTechnology());
	if (fpga == nullptr) {
		// no fpga specified or not targeting an fpga
	}
	else {
		if (fpga->getFamily() != "Agilex")
			file << "derive_pll_clocks\n";
	}

	file << "derive_clock_uncertainty\n";
}

void IntelQuartus::writeConstraintFile(vhdl::VHDLExport &vhdlExport, const hlim::Circuit &circuit, std::string_view filename)
{
	auto fileHandle = vhdlExport.getDestination().writeFile(filename);
	auto &file = fileHandle->stream();

	for (auto& node : circuit.getNodes()) {
		if (auto* cdcNode = dynamic_cast<hlim::Node_CDC*>(node.get()))
		{
			vhdlExport.getAST()->isPartOfExport(cdcNode);

			// exclude cdcNodes with virtual clocks
			if (cdcNode->getClocks()[0]->getName() == "PinSplitDummyClock" or cdcNode->getClocks()[1]->getName() == "PinSplitDummyClock")
				continue;
			// exclude cdcNodes that got the same input and output clock
			if (cdcNode->getClocks()[0]->getClockPinSource() == cdcNode->getClocks()[1]->getClockPinSource())
				continue;

			// find all input registers
			std::vector<std::string> cdcIn;
			utils::UnstableSet<hlim::NodePort> alreadyVisited;
			for (auto nh : cdcNode->exploreInput(0)) {
				if (alreadyVisited.contains(nh.nodePort())) {
					nh.backtrack();
					continue;
				}
				alreadyVisited.insert(nh.nodePort());

				if (nh.isNodeType<hlim::Node_Register>()) {

					std::vector<vhdl::BaseGrouping*> regOutputReversePath;
					if (!vhdlExport.getAST()->findLocalDeclaration(nh.nodePort(), regOutputReversePath))
						continue;
					std::string regOutputIdentifier;
					for (size_t i = regOutputReversePath.size() - 2; i < regOutputReversePath.size(); --i)
						regOutputIdentifier += regOutputReversePath[i]->getInstanceName() + '|';
					regOutputIdentifier += regOutputReversePath.front()->getNamespaceScope().get(nh.nodePort()).name;
					auto type = nh.node()->getOutputConnectionType(0);
					if (type.isBitVec())
						regOutputIdentifier += "[*]";

					cdcIn.push_back(regOutputIdentifier);
					nh.backtrack();
				}

			}
			// find all output registers
			std::vector<std::string> cdcOut;
			for (auto nh : cdcNode->exploreOutput(0)) {
				if (alreadyVisited.contains(nh.nodePort())) {
					nh.backtrack();
					continue;
				}
				alreadyVisited.insert(nh.nodePort());

				if (nh.isNodeType<hlim::Node_Register>()) {

					std::vector<vhdl::BaseGrouping*> regOutputReversePath;
					if (!vhdlExport.getAST()->findLocalDeclaration(nh.nodePort(), regOutputReversePath))
						continue;
					std::string regOutputIdentifier;
					for (size_t i = regOutputReversePath.size() - 2; i < regOutputReversePath.size(); --i)
						regOutputIdentifier += regOutputReversePath[i]->getInstanceName() + '|';
					regOutputIdentifier += regOutputReversePath.front()->getNamespaceScope().get(nh.nodePort()).name;
					auto type = nh.node()->getOutputConnectionType(0);
					if (type.isBitVec())
						regOutputIdentifier += "[*]";

					cdcOut.push_back(regOutputIdentifier);
					nh.backtrack();
				}

			}
			// write constraints to file
			for(auto itIn : cdcIn)
				for (auto itOut : cdcOut)
				{
					file << "set_max_skew -get_skew_value_from_clock_period min_clock_period -skew_value_multiplier 0.8 -from [get_registers " + itIn + "] -to [get_registers " + itOut + "]\n";
					file << "set_net_delay -max -get_value_from_clock_period dst_clock_period -value_multiplier 0.8 -from [get_registers " + itIn + "] -to [get_registers " + itOut + "]\n";
				}

		}
		if (auto* portNode = dynamic_cast<hlim::Node_Pin*>(node.get()))
		{
			if (portNode->getPortDelay().numerator() > 0)
			{
				auto del = portNode->getPortDelay().numerator() * 1'000'000'000.0 / portNode->getPortDelay().denominator();
				if (portNode->isInputPin())
					file << "set_input_delay -clock " + portNode->getClocks().front()->getName() + " " + std::to_string(del) + " " + portNode->getName() + "\n";
				else
					file << "set_output_delay -clock " + portNode->getClocks().front()->getName() + " " + std::to_string(del) + " " + portNode->getName() + "\n";
			}
			else
			{
				std::string direction;
				if (portNode->isInputPin())
					direction = "Input";
				else
					direction = "Output";
				//HCL_DESIGNCHECK_HINT(false, direction + " pin " + portNode->getName() + " has no delay setting!\n");
			}
		}
	}



	

}

void IntelQuartus::writeVhdlProjectScript(vhdl::VHDLExport &vhdlExport, std::string_view filename)
{
	auto fileHandle = vhdlExport.getDestination().writeFile(filename);
	auto &file = fileHandle->stream();

	file << R"(
# This script is intended for adding the core to an existing project
#	 1. Open the quartus tcl console (View->Utility Windows->Tcl Console) 
#	 2. If necessary, change the current working directory to project directory ("cd [get_project_directory]")
#	 3. Source this script ("source path/to/this/script.tcl"). Use a relative path to this script if you want files to be added with relative paths (recommended).


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
		file << "	set_global_assignment -name SDC_FILE $directory/" << sdcFile << '\n';

	for (std::filesystem::path& vhdl_file : sourceFiles(vhdlExport, true, false))
	{
		file << "	set_global_assignment -name VHDL_FILE -hdl_version VHDL_2008 $directory/" << vhdl_file.string();
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

			std::filesystem::path path = filename;
			path.replace_extension(".qpf");

			vhdlExport.getDestination().writeFile(path);
		}

		auto fileHandle = vhdlExport.getDestination().writeFile(filename);
		auto &file = fileHandle->stream();


		file << R"(
set_global_assignment -name PROJECT_OUTPUT_DIRECTORY output_files
set_global_assignment -name VHDL_INPUT_VERSION VHDL_2008
set_instance_assignment -name VIRTUAL_PIN ON -to *
set_global_assignment -name ALLOW_REGISTER_RETIMING OFF
set_global_assignment -name NUM_PARALLEL_PROCESSORS ALL
)";

		if (auto *fpga = DesignScope::get()->getTargetTechnology<scl::arch::FPGADevice>()) {
			file << "set_global_assignment -name DEVICE " << fpga->getDevice() << "\n";
			file << "set_global_assignment -name FAMILY \"" << fpga->getFamily() << "\"\n";
		}

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
		auto relativePath = std::filesystem::relative(vhdlExport.getDestinationPath(), vhdlExport.getTestbenchDestinationPath());

		for (std::filesystem::path& tb : sourceFiles(vhdlExport, false, true))
		{
			const std::string top = tb.stem().string();
			std::string_view library = vhdlExport.getName().empty() ? 
				std::string_view{ "work" } : 
				vhdlExport.getName();

			auto fileHandle = vhdlExport.getDestination().writeFile(std::string("modelsim_") + top + ".do");
			auto &file = fileHandle->stream();

			for (std::filesystem::path& source : sourceFiles(vhdlExport, true, false))
				file << "vcom -quiet -2008 -createlib -work " << library << " " << escapeTcl((relativePath/source).string()) << '\n';
			file << "vcom -quiet -2008 -work " << library << " " << escapeTcl(tb.string()) << '\n';

			file << "vsim -t fs " << library << "." << top << '\n';
			file << "set StdArithNoWarnings 1\n";
			file << "set NumericStdNoWarnings 1\n";
			file << "add wave *\n";
			file << "config wave -signalnamewidth 1\n";
			file << "run -all\n";
		}
	}



	void IntelQuartus::prepareCircuit(hlim::Circuit &circuit)
	{
		// Implement workarounds for quartus bugs

		workaroundEntityInOut08Bug(circuit);
		workaroundReadOut08Bug(circuit);
	}

	/**
	 * @details Considering an entity A, a signal sometimes originates in a sub-entity of A and is consumed simultaneously in the parent of A as well as in another sub-entity of A.
	 * Entity A never touches this signal except to wire it around. With regards to production and consumption in the parent, entity A can declare an output port signal and bind
	 * it to the out-port of the producing sub-entity . However, Intel Quartus does not allow this output port signal to be bound to the input of the consuming sub-entity.
	 * As a work around, we insert a named signal node which forces a local signal in the vhdl output to bridge things.
	 * This is meant to work together with workaroundReadOut08Bug and must run before.
	 */
	void IntelQuartus::workaroundEntityInOut08Bug(hlim::Circuit &circuit) const
	{
		for (const auto &node : circuit.getNodes()) {
			for (auto outIdx : utils::Range(node->getNumOutputPorts())) {
				hlim::NodePort driver{node.get(), outIdx};

				// Do two consumers exist which are both in different entities (which are also different from the producer).
				utils::StableSet<hlim::NodeGroup*> nodeGroups;
				nodeGroups.insert(node->getGroup());

				for (const auto &np : node->getDirectlyDriven(outIdx))
					nodeGroups.insert(np.node->getGroup());

				if (nodeGroups.size() >= 3) {

					// Find the central entity to which one is a sub entity but another is reached through the parent.
					// This can be at any level of the hierarchy, so we have to walk all the way to the top.
					// It might also happen multiple times, so we can't just stop at the first occurrence.
					hlim::NodeGroup *prevEntity = node->getGroup();
					auto *centralEntity = prevEntity->getParent();
					while (centralEntity != nullptr) {
						bool anyIsChild = false;
						bool anyIsNotChild = false;
						for (auto ng : nodeGroups)
							if (ng != prevEntity && !ng->isChildOf(prevEntity)) {
								if (ng->isChildOf(centralEntity))
									anyIsChild = true;
								else
									anyIsNotChild = true;
							}
						
						// One consumer must be a child, one must be through the parent
						if (anyIsChild && anyIsNotChild) {
							auto directlyDriven = driver.node->getDirectlyDriven(driver.port);

							auto *signalNode = circuit.createNode<hlim::Node_Signal>();
							auto name = driver.node->attemptInferOutputName(driver.port);
							if (!name.empty() && name.size() < 300)
								signalNode->setName(name+"_workaroundEntityInOut08Bug");
							else
								signalNode->setName("workaroundEntityInOut08Bug");
							signalNode->recordStackTrace();
							signalNode->moveToGroup(centralEntity);
							signalNode->connectInput(driver);

							driver = {.node = signalNode, .port = 0ull};

							for (const auto &d : directlyDriven) {
								//if (d.node->getGroup()->isChildOf(centralEntity))
								if (d.node->getGroup() != prevEntity && !d.node->getGroup()->isChildOf(prevEntity))
									d.node->rewireInput(d.port, driver);
							}

							dbg::log(dbg::LogMessage{} << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
									<< "Applying workaround for intel quartus entity in out port signal incompatibilities to " << node.get() << " port " << outIdx << " by inserting " << signalNode << " in " << centralEntity);
						}
						
						prevEntity = centralEntity;
						centralEntity = centralEntity->getParent();
					}
				}
			}
		}
	}

	/**
	 * @details Split/duplicate signal nodes feeding into lower and higher areas of the hierarchy.
	 * 
	 * When generating a signal in any given area, it is possible to feed that signal to the parent (an output of said area)
	 * and simultaneously to a child area (an input to the child area).
	 * By default, the VHDL exporter declares this signal as an output signal (part of the port map) and feeds that signal to the 
	 * child area as well. While this is ok with ghdl, intel quartus does not accept this, so we have to duplicate the signal for quartus in
	 * order to ensure that a local, non-port-map signal gets bound to the child area.
	 */
	void IntelQuartus::workaroundReadOut08Bug(hlim::Circuit &circuit) const
	{
		std::vector<hlim::NodePort> higherDriven;
		for (auto idx : utils::Range(circuit.getNodes().size())) {
			auto node = circuit.getNodes()[idx].get();

			for (auto outIdx : utils::Range(node->getNumOutputPorts())) {
				higherDriven.clear();
				bool consumedHigher = false;
				bool consumedLocal = false;

				for (auto driven : node->getDirectlyDriven(outIdx)) {
					if (driven.node->getGroup() == node->getGroup() || (driven.node->getGroup() != nullptr && driven.node->getGroup()->isChildOf(node->getGroup()))) {
						consumedLocal = true;
					} else {
						consumedHigher = true;
						higherDriven.push_back(driven);
					}
				}

				if (consumedHigher && consumedLocal) {
					auto *sigNode = circuit.createNode<hlim::Node_Signal>();
					auto name = node->attemptInferOutputName(outIdx);
					if (!name.empty() && name.size() < 300)
						sigNode->setName(name+"_workaroundReadOut08Bug");
					else
						sigNode->setName("workaroundReadOut08Bug");
					sigNode->moveToGroup(node->getGroup());
					sigNode->connectInput({.node = node, .port = outIdx});
					sigNode->recordStackTrace();

					for (auto driven : higherDriven)
						driven.node->rewireInput(driven.port, {.node = sigNode, .port = 0});
				}
			}
		}
	}

}
