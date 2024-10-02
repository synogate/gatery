/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2022 Michael Offel, Andreas Ley

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

#include "DesignScope.h"

#include "FrontendUnitTestSimulationFixture.h"

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/frontend/Clock.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <functional>
#include <filesystem>
#include <regex>


namespace gtry {

	class GHDLGlobalFixture {
		public:
			GHDLGlobalFixture();
			~GHDLGlobalFixture();

			static bool hasGHDL() { return !m_ghdlExecutable.empty(); }
			static bool hasIntelLibrary() { return !m_intelLibrary.empty(); }
			static bool hasXilinxLibrary() { return !m_xilinxLibrary.empty(); }
			static const boost::filesystem::path &getGHDL() { return m_ghdlExecutable; }
			static const std::filesystem::path &getIntelLibrary() { return m_intelLibrary; }
			static const std::filesystem::path &getXilinxLibrary() { return m_xilinxLibrary; }
			static const std::vector<std::string> &GHDLArgs() { return m_ghdlArgs; }

		protected:
			static boost::filesystem::path m_ghdlExecutable;
			static std::filesystem::path m_intelLibrary;
			static std::filesystem::path m_xilinxLibrary;
			static std::vector<std::string> m_ghdlArgs;
	};

	/**
	 * @brief Helper class to facilitate writing unit tests
	 */
	class GHDLTestFixture : protected BoostUnitTestSimulationFixture
	{
		public:
			sim::Simulator &getSimulator() { return BoostUnitTestSimulationFixture::getSimulator(); }
			DesignScope &getDesign() { return design; }
			void stopTest() { UnitTestSimulationFixture::stopTest(); }

			GHDLTestFixture();
			~GHDLTestFixture();

			enum Flavor {
				TARGET_GHDL,
				TARGET_QUARTUS,
			};

			void addCustomVHDL(std::string name, std::string content);

			void testCompilation(Flavor flavor = TARGET_GHDL);
			void runTest(const hlim::ClockRational &timeoutSeconds);
			bool exportContains(const std::regex &regex);

			vhdl::OutputMode vhdlOutputMode = vhdl::OutputMode::AUTO;
		protected:
			std::filesystem::path m_cwd;
			std::vector<std::string> m_ghdlArgs;
			std::map<std::string, std::string> m_customVhdlFiles;
			std::vector<std::filesystem::path> m_generatedSourceFiles;

			void prepRun() override;
			void performVhdlExport(Flavor flavor = TARGET_GHDL);
	};

}
