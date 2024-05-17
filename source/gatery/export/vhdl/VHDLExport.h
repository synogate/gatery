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
#pragma once

#include "CodeFormatting.h"

#include "TestbenchRecorder.h"
#include "BaseTestbenchRecorder.h"
#include "AST.h"

#include "InterfacePackage.h"

#include "../../hlim/Circuit.h"

#include <filesystem>
#include <memory>
#include <optional>

namespace gtry {
	class SynthesisTool;
}

namespace gtry::sim {
	class Simulator;
}

namespace gtry::utils {
	class DiskFileSystem;
}

namespace gtry::vhdl {

/**
 * @todo write docs
 */
class VHDLExport
{
	public:

		VHDLExport(std::filesystem::path destination, bool rewriteUnchangedFiles = false);
		VHDLExport(std::filesystem::path destination, std::filesystem::path destinationTestbench, bool rewriteUnchangedFiles = false);
		~VHDLExport();


		VHDLExport &outputMode(OutputMode outputMode);
		VHDLExport &targetSynthesisTool(SynthesisTool *synthesisTool);		
		VHDLExport &setFormatting(CodeFormatting *codeFormatting);
		VHDLExport &writeClocksFile(std::string filename);
		VHDLExport &writeConstraintsFile(std::string filename);
		VHDLExport &writeProjectFile(std::string filename);
		VHDLExport &writeStandAloneProjectFile(std::string filename);
		VHDLExport &writeInstantiationTemplateVHDL(std::filesystem::path filename);
		CodeFormatting *getFormatting();

		VHDLExport& setLibrary(std::string name) { m_library = std::move(name); return *this; }
		std::string_view getName() const { return m_library; }

		void operator()(hlim::Circuit &circuit);

		AST *getAST() { return m_ast.get(); }
		const AST *getAST() const { return m_ast.get(); }
		utils::FileSystem &getDestination();
		std::filesystem::path getDestinationPath() const;
		utils::FileSystem &getTestbenchDestination();
		std::filesystem::path getTestbenchDestinationPath() const;
		std::filesystem::path getSingleFileFilename() { return m_singleFileName; }
		bool isSingleFileExport();

		void addTestbenchRecorder(sim::Simulator &simulator, const std::string &name, bool inlineTestData = false);

		inline const std::vector<std::unique_ptr<BaseTestbenchRecorder>> &getTestbenchRecorder() const { return m_testbenchRecorder; }

		inline const std::string &getProjectFilename() const { return m_projectFilename; }
		inline const std::string &getConstraintsFilename() const { return m_constraintsFilename; }
		inline const std::string &getClocksFilename() const { return m_clocksFilename; }

		InterfacePackageContent &getInterfacePackage() { return m_interfacePackageContent; }

		void addCustomVhdlFile(std::string name, std::string content) { m_customVhdlFiles[std::move(name)] = std::move(content); }
		inline const std::map<std::string, std::string> &getCustomVHDLFiles() const { return m_customVhdlFiles; }
	protected:
		OutputMode m_outputMode = OutputMode::AUTO;
		std::filesystem::path m_singleFileName;

		std::unique_ptr<utils::DiskFileSystem> m_fileSystem;
		std::unique_ptr<utils::DiskFileSystem> m_fileSystemTestbench;
		std::unique_ptr<CodeFormatting> m_codeFormatting;
		std::unique_ptr<SynthesisTool> m_synthesisTool;
		std::vector<std::unique_ptr<BaseTestbenchRecorder>> m_testbenchRecorder;
		std::unique_ptr<AST> m_ast;
		InterfacePackageContent m_interfacePackageContent;
		std::string m_library;

		std::string m_projectFilename;
		std::string m_standAloneProjectFilename;
		std::string m_constraintsFilename;
		std::string m_clocksFilename;
		std::filesystem::path m_instantiationTemplateVHDL;

		struct TestbenchRecorderSettings {
			sim::Simulator *simulator;
			std::string name;
			bool inlineTestData;
		};
		std::vector<TestbenchRecorderSettings> m_testbenchRecorderSettings;

		std::map<std::string, std::string> m_customVhdlFiles;

		void doWriteInstantiationTemplateVHDL(std::filesystem::path destination);
};

}
