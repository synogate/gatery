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
#include "OSERDESE2.h"

namespace gtry::scl::arch::xilinx {

OSERDESE2::OSERDESE2(unsigned width)
{
    m_libraryName = "UNISIM";
    m_name = "OSERDESE2";
    m_genericParameters["DATA_RATE_OQ"] = "\"DDR\"";    // DDR, SDR
    m_genericParameters["DATA_RATE_TQ"] = "\"DDR\"";    // DDR, BUF, SDR
    HCL_DESIGNCHECK_HINT((width >= 2 && width <= 8) || (width == 10 || width == 14), "Invalid bit width of OSERDESE2: Valid widths are: 2-8,10,14");
    m_genericParameters["DATA_WIDTH"] = boost::lexical_cast<std::string>(width);           // Parallel data width (2-8,10,14)
    m_genericParameters["INIT_OQ"] = "'0'";             // Initial value of OQ output (1'b0,1'b1)
    m_genericParameters["INIT_TQ"] = "'0'";             // Initial value of TQ output (1'b0,1'b1)
    m_genericParameters["SERDES_MODE"] = "\"MASTER\"";  // MASTER, SLAVE
    m_genericParameters["SRVAL_OQ"] = "'0'",            // OQ output value when SR is used (1'b0,1'b1)
    m_genericParameters["SRVAL_TQ"] = "'0'",            // TQ output value when SR is used (1'b0,1'b1)
    m_genericParameters["TBYTE_CTL"] = "\"FALSE\"";     // Enable tristate byte operation (FALSE, TRUE)
    m_genericParameters["TBYTE_SRC"] = "\"FALSE\"";     // Tristate byte source (FALSE, TRUE)
    m_genericParameters["TRISTATE_WIDTH"] = "1";        // 3-state converter width (1,4)


    m_clockNames = {"CLK", "CLKDIV"};
    m_resetNames = {"", "RST"};
    m_clocks.resize(CLK_COUNT);

    resizeInputs(IN_COUNT);
    resizeOutputs(OUT_COUNT);
    for (auto i : utils::Range(OUT_COUNT))
        setOutputConnectionType(i, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
}

void OSERDESE2::setSlave()
{
    m_genericParameters["SERDES_MODE"] = "\"SLAVE\"";  // MASTER, SLAVE
}

std::string OSERDESE2::getTypeName() const
{
    return "OSERDESE2";
}

void OSERDESE2::assertValidity() const
{
}

std::string OSERDESE2::getInputName(size_t idx) const
{
    switch (idx) {
        // D1 - D8: 1-bit (each) input: Parallel data inputs (1-bit each)
        case IN_D1: return "D1";
        case IN_D2: return "D2";
        case IN_D3: return "D3";
        case IN_D4: return "D4";
        case IN_D5: return "D5";
        case IN_D6: return "D6";
        case IN_D7: return "D7";
        case IN_D8: return "D8";
        case IN_OCE: return "OCE"; // 1-bit input: Output data clock enable
        // SHIFTIN1 / SHIFTIN2: 1-bit (each) input: Data input expansion (1-bit each)
        case IN_SHIFTIN1: return "SHIFTIN1";
        case IN_SHIFTIN2: return "SHIFTIN2";
        // T1 - T4: 1-bit (each) input: Parallel 3-state inputs
        case IN_T1: return "T1";
        case IN_T2: return "T2";
        case IN_T3: return "T3";
        case IN_T4: return "T4";
        case IN_TBYTEIN: return "TBYTEIN"; // 1-bit input: Byte group tristate
        case IN_TCE: return "TCE";     // 1-bit input: 3-state clock enable
        default: return "";
    }
}

std::string OSERDESE2::getOutputName(size_t idx) const
{
    switch (idx) {
        case OUT_OFB: return "OFB"; // 1-bit output: Feedback path for data
        case OUT_OQ: return "OQ";  // 1-bit output: Data path output
        // SHIFTOUT1 / SHIFTOUT2: 1-bit (each) output: Data output expansion (1-bit each)
        case OUT_SHIFTOUT1: return "SHIFTOUT1";
        case OUT_SHIFTOUT2: return "SHIFTOUT2";
        case OUT_TBYTEOUT: return "TBYTEOUT"; // 1-bit output: Byte group tristate
        case OUT_TFB: return "TFB"; // 1-bit output: 3-state control
        case OUT_TQ: return "TQ"; // 1-bit output: 3-state control
        default:
            return "invalid";
    }
}

void OSERDESE2::setInput(Inputs input, const Bit &bit)
{
    rewireInput(input, bit.getReadPort());
}

Bit OSERDESE2::getOutput(Outputs output)
{
    return Bit(SignalReadPort(hlim::NodePort{.node = this, .port = (size_t)output}));
}


std::unique_ptr<hlim::BaseNode> OSERDESE2::cloneUnconnected() const
{
    OSERDESE2 *ptr;
    std::unique_ptr<BaseNode> res(ptr = new OSERDESE2(0)); // width replaced by copying generic parameters
    copyBaseToClone(res.get());

    ptr->m_libraryName = m_libraryName;
    ptr->m_name = m_name;
    ptr->m_genericParameters = m_genericParameters;

    return res;
}

std::string OSERDESE2::attemptInferOutputName(size_t outputPort) const
{
    return "TODO_infer_name";
}



}
