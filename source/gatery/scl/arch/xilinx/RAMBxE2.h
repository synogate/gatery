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

namespace gtry::scl::arch::xilinx {

class RAMBxE2 : public gtry::hlim::Node_External
{
    public:
		enum Type {
			RAMB18E2,
			RAMB36E2,
		};

        enum Clocks {
            CLK_A_RD,
            CLK_B_WR,
            CLK_COUNT
        };

        enum Inputs {
			// 18k
			IN_ADDR_A_RDADDR,
			IN_ADDR_B_WRADDR,
            IN_ADDREN_A,
            IN_ADDREN_B,

            IN_CAS_DIMUX_A,
            IN_CAS_DIMUX_B,
			IN_CAS_DIN_A,
			IN_CAS_DIN_B,
			IN_CAS_DINP_A,
			IN_CAS_DINP_B,

            IN_CAS_DOMUX_A,
            IN_CAS_DOMUX_B,
            IN_CAS_DOMUXEN_A,
            IN_CAS_DOMUXEN_B,

			IN_DIN_A_DIN,
			IN_DIN_B_DIN,
			IN_DINP_A_DINP,
			IN_DINP_B_DINP,

            IN_EN_A_RD_EN,
            IN_EN_B_WR_EN,
			
            IN_REG_CE_A_REG_CE,
            IN_REG_CE_B,

			IN_RST_RAM_A_RST_RAM,
			IN_RST_RAM_B,
			IN_RST_REG_A_RST_REG,
			IN_RST_REG_B,

            IN_SLEEP,
		
			IN_WE_A,
			IN_WE_B_WE,
			
			// 36k

			IN_CAS_IND_BITERR,
			IN_CAS_INS_BITERR,
			IN_CAS_OREG_IMUX_A,
			IN_CAS_OREG_IMUX_B,
			IN_CAS_OREG_IMUXEN_A,
			IN_CAS_OREG_IMUXEN_B,

			IN_ECC_PIPE_CE,
			IN_INJECT_D_BITERR,
			IN_INJECT_S_BITERR,
			/*
            IN_COUNT_18 = IN_WE_B_WE + 1,
			IN_COUNT_36 = IN_INJECT_S_BITERR + 1,
			*/
			IN_COUNT
        };
        enum Outputs {
			// 18k
			OUT_CAS_DOUT_A,
			OUT_CAS_DOUT_B,
			OUT_CAS_DOUTP_A,
			OUT_CAS_DOUTP_B,
			OUT_DOUT_A_DOUT,
			OUT_DOUT_B_DOUT,
			OUT_DOUTP_A_DOUTP,
			OUT_DOUTP_B_DOUTP,

			// 36k
			OUT_CAS_OUTD_BITERR,
			OUT_CAS_OUTS_BITERR,
			OUT_D_BITERR,
			OUT_ECC_PARITY,
			OUT_RD_ADDR_ECC,
			OUT_S_BITERR,

			/*
            OUT_COUNT_18 = OUT_DOUTP_B_DOUTP+1,
            OUT_COUNT_36 = OUT_S_BITERR+1,
			*/
			OUT_COUNT
        };

		enum class CascadeOrder {
			NONE,
			FIRST,
            MIDDLE,
			LAST,
		};

		enum class ClockDomains {
			COMMON,
			INDEPENDENT,
		};

		enum class WriteMode {
			NO_CHANGE,
			READ_FIRST,
			WRITE_FIRST,
		};

		struct PortSetup {
			WriteMode writeMode = WriteMode::NO_CHANGE;
			CascadeOrder cascadeOrder = CascadeOrder::NONE;
			size_t readWidth = 0u;
			size_t writeWidth = 0u;
			bool outputRegs = false;
		};

        RAMBxE2(Type type);

		void defaultInputs(bool writePortA, bool writePortB);

		RAMBxE2 &setupClockDomains(ClockDomains clkDom);

		RAMBxE2 &setupPortA(PortSetup portSetup);
		RAMBxE2 &setupPortB(PortSetup portSetup);

		RAMBxE2 &setupCascadeOrderA(CascadeOrder cascadeOrder);
		RAMBxE2 &setupCascadeOrderB(CascadeOrder cascadeOrder);

		//RAMBxE2 &setupInitData(const sim::DefaultBitVectorState &init, bool useParity = false);

		void connectInput(Inputs input, const Bit &bit);
		void connectInput(Inputs input, const BVec &bvec);
		BVec getOutputBVec(Outputs output);
		Bit getOutputBit(Outputs output);

		BVec getReadData(size_t width, bool portA);
		BVec getReadDataPortA(size_t width) { return getReadData(width, true); }
		BVec getReadDataPortB(size_t width) { return getReadData(width, false); }

		void connectWriteData(const BVec &input, bool portA);
		void connectWriteDataPortA(const BVec &input) { connectWriteData(input, true); }
		void connectWriteDataPortB(const BVec &input) { connectWriteData(input, false); }

        virtual std::string getTypeName() const override;
        virtual void assertValidity() const override;
        virtual std::string getInputName(size_t idx) const override;
        virtual std::string getOutputName(size_t idx) const override;

        virtual std::unique_ptr<BaseNode> cloneUnconnected() const override;

        virtual std::string attemptInferOutputName(size_t outputPort) const override;
    protected:
		Type m_type;

		static std::string WriteMode2Str(WriteMode wm);
		static std::string ClockDomains2Str(ClockDomains cd);
		static std::string CascadeOrder2Str(CascadeOrder cco);
};

}

