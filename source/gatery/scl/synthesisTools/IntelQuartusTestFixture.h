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

#include <memory>
#include <functional>
#include <filesystem>
#include <regex>

namespace gtry {

/**
 * @addtogroup gtry_synthesisTools
 * @{
 */

	class IntelQuartusGlobalFixture {
		public:
			IntelQuartusGlobalFixture();
			~IntelQuartusGlobalFixture();

			static bool hasIntelQuartus() { return !m_intelQuartusBinPath.empty(); }
			static const std::filesystem::path &getIntelQuartusBinPath() { return m_intelQuartusBinPath; }
			static const std::filesystem::path &getIntelQuartusBinSynthesizer() { return m_intelQuartusBinSynthesizer; }
			static const std::filesystem::path &getIntelQuartusBinFitter() { return m_intelQuartusBinFitter; }
			static const std::filesystem::path &getIntelQuartusBinAssembler() { return m_intelQuartusBinAssembler; }
			static const std::filesystem::path &getIntelQuartusBinTimingAnalyzer() { return m_intelQuartusBinTimingAnalyzer; }
		protected:
			static std::filesystem::path m_intelQuartusBinPath;
			static std::filesystem::path m_intelQuartusBinSynthesizer;
			static std::filesystem::path m_intelQuartusBinFitter;
			static std::filesystem::path m_intelQuartusBinAssembler;
			static std::filesystem::path m_intelQuartusBinTimingAnalyzer;
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

			template<typename T>
			struct Usage {
				T inclChildren;
				T self;
			};

			struct FitterResourceUtilization {
				std::string fullHierarchyName;
				Usage<double> ALMsNeeded;
				Usage<double> ALMsInFinalPlacement;
				Usage<double> ALMsRecoverable;
				Usage<double> ALMsUnavailable;
				Usage<double> ALMsForMemory;
				Usage<size_t> combinationalALUTs;
				Usage<size_t> dedicatedLogicRegisters;
				Usage<size_t> ioRegisters;
				size_t blockMemoryBits;
				size_t M20Ks;
				size_t DSPsNeeded;
				size_t DSPsInFinalPlacement;
				size_t DSPsRecoverable;
			};
/*
			struct FitterRAMSummary {
				std::string fullHierarchyName;
				std::string type;
				std::string mode;
				std::string ClockMode;
				size_t portADepth;
				size_t portBDepth;
				size_t portBWidth;
				bool portAInputRegister;
				bool portAOutputRegister;
				bool portBInputRegister;
				bool portBOutputRegister;
				size_t size;
				size_t implementationPortADepth;
				size_t implementationPortAWidth;
				size_t implementationPortBDepth;
				size_t implementationPortBWidth;
				size_t implementationBits;
				size_t numM20K;
				size_t numMLAB;
				std::string mixedPortRDWMode;
				std::string portA_RDWMode;
				std::string portB_RDWMode;
			};
*/
			struct TimingAnalysisFMax {
				double fmax;
				double fmaxRestricted;
			};

			const std::map<std::string, IntelQuartusTestFixture::FitterResourceUtilization, std::less<>> &getFitterResourceUtilization();
//			std::vector<FitterRAMSummary> getFitterRAMSummary();

			FitterResourceUtilization getFitterResourceUtilization(std::string_view instancePath);

			const std::map<std::string, TimingAnalysisFMax> &getTimingAnalysisFmax() const { return m_timingAnalysisFmax; }
			TimingAnalysisFMax getTimingAnalysisFmax(hlim::Clock *clock) const;
			inline TimingAnalysisFMax getTimingAnalysisFmax(const gtry::Clock &clock) const { return getTimingAnalysisFmax(clock.getClk()); }
			bool timingMet(hlim::Clock *clock) const;
			inline bool timingMet(const gtry::Clock &clock) const { return timingMet(clock.getClk()); }
		protected:
			std::filesystem::path m_cwd;
			std::map<std::string, std::string> m_customVhdlFiles;
			std::vector<std::filesystem::path> m_generatedSourceFiles;

			void writeTimingToCsvTclScript();
			void parseTimingCsv();
			std::map<std::string, TimingAnalysisFMax> m_timingAnalysisFmax;
			std::optional<std::map<std::string, FitterResourceUtilization, std::less<>>> m_resourceUtilization;
	};

/**@}*/

}
