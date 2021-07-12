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

#include "DesignScope.h"

#include <gatery/simulation/UnitTestSimulationFixture.h>

#include <gatery/simulation/waveformFormats/VCDSink.h>
#include <gatery/export/vhdl/VHDLExport.h>


#include <memory>
#include <functional>
#include <filesystem>


namespace gtry {

    /**
     * @brief Helper class to facilitate writing unit tests
     */
    class UnitTestSimulationFixture : protected sim::UnitTestSimulationFixture
    {
        public:
            ~UnitTestSimulationFixture();

            /// Compiles the graph and does one combinatory evaluation
            void eval();
            /// Compiles and runs the simulation for a specified amount of ticks (rising edges) of the given clock.
            void runTicks(const hlim::Clock* clock, unsigned numTicks);

            /// Enables recording of a waveform for a subsequent simulation run
            void recordVCD(const std::filesystem::path &destination);
            /// Exports as VHDL and (optionally) writes a vhdl tes6tbench of the subsequent simulation run
            void outputVHDL(const std::filesystem::path &destination, bool includeTest = true);

            /// Stops an ongoing simulation (to be used during runHitsTimeout)
            void stopTest();

            /// Compiles and runs the simulation until the timeout (in simulation time) is reached or stopTest is called
            /// @return returns true if the timeout was reached.
            bool runHitsTimeout(const hlim::ClockRational &timeoutSeconds);

            DesignScope design;
        protected:
            bool m_stopTestCalled = false;
            std::optional<sim::VCDSink> m_vcdSink;
            std::optional<vhdl::VHDLExport> m_vhdlExport;
    };

    struct BoostUnitTestGlobalFixture {
        void setup();

        static std::filesystem::path graphVisPath;
        static std::filesystem::path vcdPath;
        static std::filesystem::path vhdlPath;
    };


    /**
     * @brief Helper class to facilitate writing unit tests
     */
    class BoostUnitTestSimulationFixture : protected UnitTestSimulationFixture {
        public:
            void runFixedLengthTest(const hlim::ClockRational &seconds);
            void runEvalOnlyTest();
            void runTest(const hlim::ClockRational &timeoutSeconds);
        protected:
            void prepRun();
    };

}
