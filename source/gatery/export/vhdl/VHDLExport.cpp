#include "VHDLExport.h"
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
#include "VHDLExport.h"

#include "Package.h"
#include "InterfacePackage.h"
#include "Entity.h"

#include "TestbenchRecorder.h"
#include "FileBasedTestbenchRecorder.h"

#include "../../utils/Range.h"
#include "../../utils/Enumerate.h"
#include "../../utils/Exceptions.h"
#include "../../utils/FileSystem.h"

#include "../../hlim/coreNodes/Node_Arithmetic.h"

#include "../../hlim/coreNodes/Node_Compare.h"
#include "../../hlim/coreNodes/Node_Constant.h"
#include "../../hlim/coreNodes/Node_Logic.h"
#include "../../hlim/coreNodes/Node_Multiplexer.h"
#include "../../hlim/coreNodes/Node_Signal.h"
#include "../../hlim/coreNodes/Node_Register.h"
#include "../../hlim/coreNodes/Node_Rewire.h"
#include "../../hlim/coreNodes/Node_Pin.h"

#include "../../hlim/supportNodes/Node_PathAttributes.h"


#include "../../simulation/Simulator.h"

#include "../../frontend/SynthesisTool.h"

#include <gatery/utils/FileSystem.h>


#include <set>
#include <map>
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <functional>
#include <list>
#include <map>

namespace gtry::vhdl {


VHDLExport::VHDLExport(std::filesystem::path destination, bool rewriteUnchangedFiles)
{
	if (destination.has_extension()) {
		m_singleFileName = destination.filename();
		
		std::filesystem::path dir = destination.parent_path();
		if (dir.empty())
			dir = ".";

		m_fileSystem = std::make_unique<utils::DiskFileSystem>(dir, !rewriteUnchangedFiles);
		m_fileSystemTestbench = std::make_unique<utils::DiskFileSystem>(dir);
	} else {
		m_fileSystem = std::make_unique<utils::DiskFileSystem>(destination, !rewriteUnchangedFiles);
		m_fileSystemTestbench = std::make_unique<utils::DiskFileSystem>(destination);
	}

	m_codeFormatting.reset(new DefaultCodeFormatting());
	m_synthesisTool.reset(new DefaultSynthesisTool());
}


VHDLExport::VHDLExport(std::filesystem::path destination, std::filesystem::path destinationTestbench, bool rewriteUnchangedFiles)
{
	if (destination.has_extension()) {
		m_singleFileName = destination.filename();
		
		std::filesystem::path dir = destination.parent_path();
		if (dir.empty())
			dir = ".";

		m_fileSystem = std::make_unique<utils::DiskFileSystem>(dir, !rewriteUnchangedFiles);
	} else
		m_fileSystem = std::make_unique<utils::DiskFileSystem>(destination);

	m_fileSystemTestbench = std::make_unique<utils::DiskFileSystem>(destinationTestbench);

	m_codeFormatting.reset(new DefaultCodeFormatting());
	m_synthesisTool.reset(new DefaultSynthesisTool());
}

VHDLExport::~VHDLExport()
{
	clearTestbenchRecorder(); // Explicitly clear to trigger destructors and flushes before anything important destructs.
}

VHDLExport &VHDLExport::outputMode(OutputMode outputMode)
{
	m_outputMode = outputMode;
	return *this;
}

VHDLExport &VHDLExport::targetSynthesisTool(SynthesisTool *synthesisTool)
{
	m_synthesisTool.reset(synthesisTool);
	return *this;
}


VHDLExport &VHDLExport::setFormatting(CodeFormatting *codeFormatting)
{
	m_codeFormatting.reset(codeFormatting);
	return *this;
}

VHDLExport &VHDLExport::writeClocksFile(std::string filename)
{
	m_clocksFilename = std::move(filename);
	return *this;
}

VHDLExport &VHDLExport::writeConstraintsFile(std::string filename)
{
	m_constraintsFilename = std::move(filename);
	return *this;
}

VHDLExport &VHDLExport::writeProjectFile(std::string filename)
{
	m_projectFilename = std::move(filename);
	return *this;
}

VHDLExport& VHDLExport::writeStandAloneProjectFile(std::string filename)
{
	m_standAloneProjectFilename = std::move(filename);
	return *this;
}


VHDLExport& VHDLExport::writeInstantiationTemplateVHDL(std::filesystem::path filename)
{
	m_instantiationTemplateVHDL = std::move(filename);
	return *this;
}




CodeFormatting *VHDLExport::getFormatting()
{
	return m_codeFormatting.get();
}

void VHDLExport::operator()(hlim::Circuit &circuit)
{
	m_synthesisTool->prepareCircuit(circuit);

	m_ast.reset(new AST(m_codeFormatting.get(), m_synthesisTool.get()));
	if (!m_interfacePackageContent.empty())
		m_ast->generateInterfacePackage(m_interfacePackageContent);

	m_ast->convert((hlim::Circuit &)circuit);
	m_ast->writeVHDL(*m_fileSystem, m_outputMode, m_singleFileName, m_customVhdlFiles);

	for (auto &e : m_testbenchRecorderSettings) {
		if (e.inlineTestData)
			m_testbenchRecorder.push_back(std::make_unique<TestbenchRecorder>(*this, m_ast.get(), *e.simulator, *m_fileSystemTestbench, e.name));
		else
			m_testbenchRecorder.push_back(std::make_unique<FileBasedTestbenchRecorder>(*this, m_ast.get(), *e.simulator, *m_fileSystemTestbench, e.name));
			
		e.simulator->addCallbacks(m_testbenchRecorder.back().get());
	}
	
	if (!m_constraintsFilename.empty())
		m_synthesisTool->writeConstraintFile(*this, circuit, m_constraintsFilename);

	if (!m_clocksFilename.empty())
		m_synthesisTool->writeClocksFile(*this, circuit, m_clocksFilename);

	if (!m_projectFilename.empty())
		m_synthesisTool->writeVhdlProjectScript(*this, m_projectFilename);

	if (!m_standAloneProjectFilename.empty())
		m_synthesisTool->writeStandAloneProject(*this, m_standAloneProjectFilename);

	if (!m_instantiationTemplateVHDL.empty())
		doWriteInstantiationTemplateVHDL(m_instantiationTemplateVHDL);
}


std::filesystem::path VHDLExport::getDestinationPath() const
{
	return m_fileSystem->basePath();
}

utils::FileSystem &VHDLExport::getDestination()
{
	return *m_fileSystem;
}

std::filesystem::path VHDLExport::getTestbenchDestinationPath() const
{
	return m_fileSystemTestbench->basePath();
}


utils::FileSystem &VHDLExport::getTestbenchDestination()
{
	return *m_fileSystemTestbench;
}


bool VHDLExport::isSingleFileExport()
{
	return !m_singleFileName.empty() && (m_outputMode == OutputMode::AUTO || m_outputMode == OutputMode::SINGLE_FILE);
}

void VHDLExport::addTestbenchRecorder(sim::Simulator &simulator, const std::string &name, bool inlineTestData)
{
	m_testbenchRecorderSettings.emplace_back(TestbenchRecorderSettings{&simulator, name, inlineTestData});
}

void VHDLExport::doWriteInstantiationTemplateVHDL(std::filesystem::path destination)
{
	CodeFormatting &cf = m_ast->getCodeFormatting();
	auto *rootEntity = m_ast->getRootEntity();

	std::vector<hlim::Clock*> clocks(rootEntity->getClocks().begin(), rootEntity->getClocks().end());
	std::vector<hlim::Clock*> resets(rootEntity->getResets().begin(), rootEntity->getResets().end());
	std::vector<hlim::Node_Pin*> ioPins(rootEntity->getIoPins().begin(), rootEntity->getIoPins().end());

	// Preserve creation order
	std::sort(ioPins.begin(), ioPins.end(), [](const hlim::Node_Pin *lhs, const hlim::Node_Pin *rhs) {
		return lhs->getId() > rhs->getId();
	});

	auto fileHandle = m_fileSystem->writeFile(destination);
	auto &file = fileHandle->stream();

	file << "library ieee;\n"
		   << "use ieee.std_logic_1164.ALL;\n"
		   << "use ieee.numeric_std.all;\n\n";

	if (!m_library.empty())
	{
		file << "library " << m_library << ";\n" <<
				"use " << m_library << "." << rootEntity->getName() << "; \n\n";
	}

	file << "entity example is\nend example;\n\n";

	file << "architecture rtl of example is\n";

	/////////////	Signals

	for (auto clock : clocks) {
		cf.indent(file, 1);
		file << "signal " << rootEntity->getNamespaceScope().getClock(clock).name << " : STD_LOGIC;\n";
	}
	file << '\n';

	for (auto clock : resets) {
		cf.indent(file, 1);
		file << "signal " << rootEntity->getNamespaceScope().getReset(clock).name << " : STD_LOGIC;\n";
	}
	file << '\n';
	file << '\n';

	for (auto ioPin : ioPins) {
		const auto &decl = rootEntity->getNamespaceScope().get(ioPin);

		cf.indent(file, 1);
		file << "signal " << decl.name << " : ";
		cf.formatConnectionType(file, decl);
		file << ';' << std::endl;
	}

	file << '\n';

	std::string fullName;
	if (!m_library.empty())
		fullName = m_library + '.' + rootEntity->getName();
	else
		fullName = "work." + rootEntity->getName();

#if 0
	/////////////	Component declaration
	cf.indent(file, 1);
	file << "component " << rootEntity->getName() << '\n';
	rootEntity->writePortDeclaration(file, 3);
	cf.indent(file, 1);
	file << "end component " << rootEntity->getName() << ";\n\n";
#endif

	file << "begin\n\n";

	/////////////	Component instantiation

	cf.indent(file, 1);
//	file << "example_instance : " << fullName << " port map (\n";
	file << "example_instance: entity " << fullName << " port map (\n";

	{
		std::vector<std::string> portmapList;

		for (auto &s : clocks) {
			const auto &name = rootEntity->getNamespaceScope().getClock(s).name;
			portmapList.push_back(name + " => " + name);
		}
		for (auto &s : resets) {
			const auto &name = rootEntity->getNamespaceScope().getReset(s).name;
			portmapList.push_back(name + " => " + name);
		}
		for (auto &s : ioPins) {
			const auto &name = rootEntity->getNamespaceScope().get(s).name;
			portmapList.push_back(name + " => " + name);
		}

		for (auto i : utils::Range(portmapList.size())) {
			cf.indent(file, 2);
			file << portmapList[i];
			if (i+1 < portmapList.size())
				file << ',';
			file << '\n';
		}
	}

	cf.indent(file, 1);
	file << ");\n\n";

	file << "end architecture;\n";

}


}
