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

class ALTDPRAM : public gtry::hlim::Node_External
{
    public:
        enum Clocks {
            INCLOCK,
            OUTCLOCK,
            CLK_COUNT
        };

        enum Inputs {
			// Bits
            IN_RDADDRESSSTALL,
            IN_WRADDRESSSTALL,
            IN_WREN,
            IN_INCLOCKEN,
            IN_RDEN,
            IN_OUTCLOCKEN,
			IN_ACLR,

			// UInts
			IN_DATA,
			IN_RDADDRESS,
			IN_WRADDRESS,
			IN_BYTEENA,

            IN_COUNT
        };
        enum Outputs {
			// UInts
            OUT_Q,

            OUT_COUNT
        };

		enum class RDWBehavior {
			DONT_CARE,
			OLD_DATA,
            CONSTRAINED_DONT_CARE,
			NEW_DATA_MASKED_UNDEFINED,
		};

		struct PortSetup {
			bool inputRegs = false;
			bool outputRegs = false;
			bool resetAddr = false;
            bool resetRdEnable = false;
			bool resetWrEn = false;
			bool resetWrData = false;
			bool outReset = false;
		};

        ALTDPRAM(size_t width, size_t depth);

		ALTDPRAM &setupReadPort(PortSetup portSetup);
		ALTDPRAM &setupWritePort(PortSetup portSetup);

        ALTDPRAM &setupMixedPortRdw(RDWBehavior rdw);

		ALTDPRAM &setupRamType(const std::string &type);
		ALTDPRAM &setupSimulationDeviceFamily(const std::string &devFamily);

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
		size_t m_width;
		size_t m_depth;

		static std::string RDWBehavior2Str(RDWBehavior rdw);
};

}

