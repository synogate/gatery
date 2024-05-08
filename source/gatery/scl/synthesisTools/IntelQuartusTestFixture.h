/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2024 Michael Offel, Andreas Ley

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

#include <gatery/frontend/DesignScope.h>

#include <gatery/frontend/FrontendUnitTestSimulationFixture.h>

#include <gatery/export/vhdl/VHDLExport.h>
#include <gatery/frontend/Clock.h>

#include <boost/filesystem.hpp>

#include <memory>
#include <functional>
#include <filesystem>
#include <regex>

namespace gtry::scl {

	class IntelQuartusGlobalFixture {
		public:
			IntelQuartusGlobalFixture();
			~IntelQuartusGlobalFixture();

			static bool hasIntelQuartus() { return !m_intelQuartusBinPath.empty(); }
			static const boost::filesystem::path &getIntelQuartusBinPath() { return m_intelQuartusBinPath; }
		protected:
			static boost::filesystem::path m_intelQuartusBinPath;
	};

	/**
	 * @brief Helper class to facilitate writing unit tests
	 */
	class IntelQuartusTestFixture : protected BoostUnitTestSimulationFixture
	{
		public:
			sim::Simulator &getSimulator() { return BoostUnitTestSimulationFixture::getSimulator(); }
			DesignScope &getDesign() { return design; }
			void stopTest() { UnitTestSimulationFixture::stopTest(); }

			IntelQuartusTestFixture();
			~IntelQuartusTestFixture();

			void addCustomVHDL(std::string name, std::string content);

			void testCompilation();
			bool exportContains(const std::regex &regex);

			vhdl::OutputMode vhdlOutputMode = vhdl::OutputMode::AUTO;
		protected:
			std::filesystem::path m_cwd;
			std::map<std::string, std::string> m_customVhdlFiles;
			std::vector<std::filesystem::path> m_generatedSourceFiles;

			void prepRun() override;
	};

}
