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

#include <gatery/frontend.h>
#include <gatery/hlim/supportNodes/Node_External.h>

namespace gtry::scl::arch::intel {

class ALTSYNCRAM : public gtry::hlim::Node_External
{
    public:
        enum Clocks {
            CLK_0,
            CLK_1,
            CLK_COUNT
        };

        enum Inputs {
			// Bits
            IN_WREN_A,
            IN_RDEN_A,
            IN_WREN_B,
            IN_RDEN_B,

			IN_CLOCKEN_0,
			IN_CLOCKEN_1,

			IN_ACLR_0,
			IN_ACLR_1,

			// UInts
			IN_DATA_A,
			IN_ADDRESS_A,
			IN_BYTEENA_A,
			IN_DATA_B,
			IN_ADDRESS_B,
			IN_BYTEENA_B,

            IN_COUNT
        };
        enum Outputs {
			// UInts
            OUT_Q_A,
            OUT_Q_B,

            OUT_COUNT
        };

		enum class RDWBehavior {
			DONT_CARE,
			OLD_DATA,
            CONSTRAINED_DONT_CARE,
			NEW_DATA_MASKED_UNDEFINED,
		};

		struct PortSetup {
			RDWBehavior rdw = RDWBehavior::DONT_CARE;
			bool inputRegs = false;
			bool outputRegs = false;
			bool dualClock = false;
			bool resetAddr = false;
			bool resetWrEn = false;
			bool outReset = false;
		};

        ALTSYNCRAM(size_t size);

		ALTSYNCRAM &setupSinglePort(); // In single port mode, only port A can be used
		ALTSYNCRAM &setupSimpleDualPort(); // port A must be the write port and port B the read port
		ALTSYNCRAM &setupTrueDualPort();
		ALTSYNCRAM &setupROM();

		ALTSYNCRAM &setupPortA(size_t width, PortSetup portSetup);
		ALTSYNCRAM &setupPortB(size_t width, PortSetup portSetup);

        ALTSYNCRAM &setupMixedPortRdw(RDWBehavior rdw);

		ALTSYNCRAM &setupRamType(const std::string &type);
		ALTSYNCRAM &setupSimulationDeviceFamily(const std::string &devFamily);

		void connectInput(Inputs input, const Bit &bit);
		void connectInput(Inputs input, const UInt &UInt);
		UInt getOutputUInt(Outputs output);

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const override;
    protected:
		size_t m_size;

		static std::string RDWBehavior2Str(RDWBehavior rdw);
};

}

