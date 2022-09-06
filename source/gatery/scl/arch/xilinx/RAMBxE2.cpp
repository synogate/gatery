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

#include "RAMBxE2.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::xilinx {

RAMBxE2::RAMBxE2(Type type) : m_type(type)
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	switch (m_type) {
		case RAMB18E2: m_name = "RAMB18E2"; break;
		case RAMB36E2: m_name = "RAMB36E2"; break;
	}
	m_isEntity = false;
	m_clockNames = {"CLKARDCLK", "CLKBWRCLK"};
	m_resetNames = {"", ""};
	m_clocks.resize(CLK_COUNT);

	
	resizeInputs(IN_COUNT);
	resizeOutputs(OUT_COUNT);

	size_t mult = 1;
	if (m_type == RAMB36E2) {
		mult = 2;

		setOutputConnectionType(OUT_ECC_PARITY, {.interpretation = hlim::ConnectionType::BITVEC, .width=8});
		setOutputConnectionType(OUT_RD_ADDR_ECC, {.interpretation = hlim::ConnectionType::BITVEC, .width=9});
	} else {

		setOutputConnectionType(OUT_ECC_PARITY, {.interpretation = hlim::ConnectionType::BITVEC, .width=0});
		setOutputConnectionType(OUT_RD_ADDR_ECC, {.interpretation = hlim::ConnectionType::BITVEC, .width=0});
	}
	setOutputConnectionType(OUT_CAS_DOUT_A, {.interpretation = hlim::ConnectionType::BITVEC, .width=16*mult});
	setOutputConnectionType(OUT_CAS_DOUT_B, {.interpretation = hlim::ConnectionType::BITVEC, .width=16*mult});
	setOutputConnectionType(OUT_CAS_DOUTP_A, {.interpretation = hlim::ConnectionType::BITVEC, .width=2*mult});
	setOutputConnectionType(OUT_CAS_DOUTP_B, {.interpretation = hlim::ConnectionType::BITVEC, .width=2*mult});
	setOutputConnectionType(OUT_DOUT_A_DOUT, {.interpretation = hlim::ConnectionType::BITVEC, .width=16*mult});
	setOutputConnectionType(OUT_DOUT_B_DOUT, {.interpretation = hlim::ConnectionType::BITVEC, .width=16*mult});
	setOutputConnectionType(OUT_DOUTP_A_DOUTP, {.interpretation = hlim::ConnectionType::BITVEC, .width=2*mult});
	setOutputConnectionType(OUT_DOUTP_B_DOUTP, {.interpretation = hlim::ConnectionType::BITVEC, .width=2*mult});

	m_genericParameters["SIM_COLLISION_CHECK"] = "GENERATE_X_ONLY";
}

void RAMBxE2::defaultInputs(bool writePortA, bool writePortB)
{
	UInt undefinedData;
	UInt undefinedParity;
	UInt undefinedAddr;

	if (m_type == RAMB18E2) {
		undefinedData = ConstUInt(BitWidth(16));
		undefinedParity = ConstUInt(BitWidth(2));
		undefinedAddr = ConstUInt(BitWidth(14));
	} else {
		undefinedData = ConstUInt(BitWidth(32));
		undefinedParity = ConstUInt(BitWidth(4));
		undefinedAddr = ConstUInt(BitWidth(15));
	}

	Bit oneB = '1';
	Bit zeroB = '0';
	Bit undefB = 'x';

	connectInput(IN_ADDREN_A, oneB);
	connectInput(IN_ADDREN_B, oneB);

	connectInput(IN_CAS_DIMUX_A, zeroB);
	connectInput(IN_CAS_DIMUX_B, zeroB);

	connectInput(IN_CAS_DIN_A, undefinedData);
	connectInput(IN_CAS_DIN_B, undefinedData);
	connectInput(IN_CAS_DINP_A, undefinedParity);
	connectInput(IN_CAS_DINP_B, undefinedParity);

	connectInput(IN_CAS_DOMUX_A, zeroB);
	connectInput(IN_CAS_DOMUX_B, zeroB);

	connectInput(IN_CAS_DOMUXEN_A, oneB);
	connectInput(IN_CAS_DOMUXEN_B, oneB);

	connectInput(IN_CAS_OREG_IMUX_A, zeroB);
	connectInput(IN_CAS_OREG_IMUX_B, zeroB);

	connectInput(IN_CAS_OREG_IMUXEN_A, oneB);
	connectInput(IN_CAS_OREG_IMUXEN_B, oneB);


	connectInput(IN_DIN_A_DIN, undefinedData);
	connectInput(IN_DIN_B_DIN, undefinedData);
	connectInput(IN_DINP_A_DINP, undefinedParity);
	connectInput(IN_DINP_B_DINP, undefinedParity);

	connectInput(IN_EN_A_RD_EN, zeroB);
	connectInput(IN_EN_B_WR_EN, zeroB);

	connectInput(IN_REG_CE_A_REG_CE, oneB);
	connectInput(IN_REG_CE_B, oneB);

	connectInput(IN_RST_RAM_A_RST_RAM, zeroB);
	connectInput(IN_RST_RAM_B, zeroB);
	connectInput(IN_RST_REG_A_RST_REG, zeroB);
	connectInput(IN_RST_REG_B, zeroB);

	connectInput(IN_SLEEP, zeroB);

	if (m_type == RAMB36E2) {
		if (writePortA)
			connectInput(IN_WE_A, "b1111");
		else
			connectInput(IN_WE_A, "b0000");

		if (writePortB)
			connectInput(IN_WE_B_WE, "b11111111");
		else
			connectInput(IN_WE_B_WE, "b00000000");
	} else {
		if (writePortA)
			connectInput(IN_WE_A, "b11");
		else
			connectInput(IN_WE_A, "b00");

		if (writePortB)
			connectInput(IN_WE_B_WE, "b1111");
		else
			connectInput(IN_WE_B_WE, "b0000");	
	}

	if (m_type == RAMB36E2) {
		connectInput(IN_CAS_IND_BITERR, undefB);
		connectInput(IN_CAS_INS_BITERR, undefB);

		connectInput(IN_ECC_PIPE_CE, zeroB);
		connectInput(IN_INJECT_D_BITERR, zeroB);
		connectInput(IN_INJECT_S_BITERR, zeroB);
	}	
}



std::string RAMBxE2::WriteMode2Str(WriteMode wm)
{
	switch (wm) {
		case WriteMode::NO_CHANGE:
			return "NO_CHANGE";
		case WriteMode::READ_FIRST:
			return "READ_FIRST";
		case WriteMode::WRITE_FIRST:
			return "WRITE_FIRST";
	}
	HCL_ASSERT_HINT(false, "Unhandled case!");
}

std::string RAMBxE2::CascadeOrder2Str(CascadeOrder cco)
{
	switch (cco) {
		case CascadeOrder::NONE:
			return "NONE";
		case CascadeOrder::FIRST:
			return "FIRST";
		case CascadeOrder::MIDDLE:
			return "MIDDLE";
		case CascadeOrder::LAST:
			return "LAST";
	}
	HCL_ASSERT_HINT(false, "Unhandled case!");
}

std::string RAMBxE2::ClockDomains2Str(ClockDomains cd)
{
	switch (cd) {
		case ClockDomains::COMMON:
			return "COMMON";
		case ClockDomains::INDEPENDENT:
			return "INDEPENDENT";
	}
	HCL_ASSERT_HINT(false, "Unhandled case!");
}


RAMBxE2 &RAMBxE2::setupPortA(PortSetup portSetup)
{
	m_genericParameters["CASCADE_ORDER_A"] = CascadeOrder2Str(portSetup.cascadeOrder);
	m_genericParameters["READ_WIDTH_A"] = portSetup.readWidth;
	m_genericParameters["WRITE_WIDTH_A"] = portSetup.writeWidth;
	m_genericParameters["WRITE_MODE_A"] = WriteMode2Str(portSetup.writeMode);

	m_genericParameters["DOA_REG"] = portSetup.outputRegs?1:0;

	m_portSetups[0] = portSetup;

	return *this;
}

RAMBxE2 &RAMBxE2::setupPortB(PortSetup portSetup)
{
	m_genericParameters["CASCADE_ORDER_B"] = CascadeOrder2Str(portSetup.cascadeOrder);
	m_genericParameters["READ_WIDTH_B"] = portSetup.readWidth;
	m_genericParameters["WRITE_WIDTH_B"] = portSetup.writeWidth;
	m_genericParameters["WRITE_MODE_B"] = WriteMode2Str(portSetup.writeMode);

	m_genericParameters["DOB_REG"] = portSetup.outputRegs?1:0;

	m_portSetups[1] = portSetup;

	return *this;
}

RAMBxE2 &RAMBxE2::setupClockDomains(ClockDomains clkDom)
{
	m_genericParameters["CLOCK_DOMAINS"] = ClockDomains2Str(clkDom);
	m_clockDomains = clkDom;
	return *this;
}


void RAMBxE2::connectInput(Inputs input, const Bit &bit)
{
	switch (input) {
		case IN_ADDREN_A:
		case IN_ADDREN_B:
		case IN_CAS_DIMUX_A:
		case IN_CAS_DIMUX_B:
		case IN_CAS_DOMUX_A:
		case IN_CAS_DOMUX_B:
		case IN_CAS_DOMUXEN_A:
		case IN_CAS_DOMUXEN_B:
		case IN_CAS_OREG_IMUX_A:
		case IN_CAS_OREG_IMUX_B:
		case IN_CAS_OREG_IMUXEN_A:
		case IN_CAS_OREG_IMUXEN_B:
		case IN_EN_A_RD_EN:
		case IN_EN_B_WR_EN:
		case IN_REG_CE_A_REG_CE:
		case IN_REG_CE_B:
		case IN_RST_RAM_A_RST_RAM:
		case IN_RST_RAM_B:
		case IN_RST_REG_A_RST_REG:
		case IN_RST_REG_B:
		case IN_SLEEP:
			NodeIO::connectInput(input, bit.readPort());
		break;

		case IN_CAS_IND_BITERR:
		case IN_CAS_INS_BITERR:
		case IN_ECC_PIPE_CE:
		case IN_INJECT_D_BITERR:
		case IN_INJECT_S_BITERR:
			HCL_DESIGNCHECK_HINT(m_type == RAMB36E2, "Input only available for RAMB32E2!");
			NodeIO::connectInput(input, bit.readPort());
		break;

		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bit to UInt input of RAMBxE2!");
	}
}

void RAMBxE2::connectInput(Inputs input, const UInt &vec)
{
	size_t mult = 1;
	if (m_type == RAMB36E2)
		mult = 2;

	switch (input) {
		case IN_ADDR_A_RDADDR:
		case IN_ADDR_B_WRADDR:
			if (m_type == RAMB36E2) {
				HCL_DESIGNCHECK_HINT(vec.size() == 15, "Data input UInt has wrong width!");
			} else
				HCL_DESIGNCHECK_HINT(vec.size() == 14, "Data input UInt has wrong width!");
		break;
		case IN_CAS_DIN_A:
		case IN_CAS_DIN_B:
			HCL_DESIGNCHECK_HINT(vec.size() == 16*mult, "Data input UInt has wrong width!");
		break;
		case IN_CAS_DINP_A:
		case IN_CAS_DINP_B:
			HCL_DESIGNCHECK_HINT(vec.size() == 2*mult, "Data input UInt has wrong width!");
		break;
		case IN_DIN_A_DIN:
		case IN_DIN_B_DIN:
			HCL_DESIGNCHECK_HINT(vec.size() == 16*mult, "Data input UInt has wrong width!");
		break;
		case IN_DINP_A_DINP:
		case IN_DINP_B_DINP:
			HCL_DESIGNCHECK_HINT(vec.size() == 2*mult, "Data input UInt has wrong width!");
		break;
		case IN_WE_A:
			HCL_DESIGNCHECK_HINT(vec.size() == 2*mult, "Data input UInt has wrong width!");
		break;
		case IN_WE_B_WE:
			HCL_DESIGNCHECK_HINT(vec.size() == 4*mult, "Data input UInt has wrong width!");
		break;		
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect UInt to bit input!");
	}
	NodeIO::connectInput(input, vec.readPort());
}

UInt RAMBxE2::getOutputUInt(Outputs output)
{
	switch (output) {
		case OUT_CAS_OUTD_BITERR:
		case OUT_CAS_OUTS_BITERR:
		case OUT_D_BITERR:
		case OUT_S_BITERR:
			HCL_DESIGNCHECK_HINT(m_type == RAMB36E2, "Trying to get output that is not available in RAMB18E2!");
			HCL_DESIGNCHECK_HINT(false, "Trying to connect UInt from bit output of RAMBxE2!");
		break;

		case OUT_ECC_PARITY:
		case OUT_RD_ADDR_ECC:
			HCL_DESIGNCHECK_HINT(m_type == RAMB36E2, "Trying to get output that is not available in RAMB18E2!");
		break;
		default:
		break;
	}

	return SignalReadPort(hlim::NodePort{this, (size_t)output});
}

Bit RAMBxE2::getOutputBit(Outputs output)
{
	switch (output) {
		case OUT_CAS_OUTD_BITERR:
		case OUT_CAS_OUTS_BITERR:
		case OUT_D_BITERR:
		case OUT_S_BITERR:
			HCL_DESIGNCHECK_HINT(m_type == RAMB36E2, "Trying to get output that is not available in RAMB18E2!");
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bit from UInt output of RAMBxE2!");
	}

	return SignalReadPort(hlim::NodePort{this, (size_t)output});
}


UInt RAMBxE2::getReadData(size_t width, bool portA)
{
	UInt result;

	// std::array<UInt, xx> is needed to ensure pack order doesn't reverse (we need to change that)

	switch (width) {
		case 72:
			HCL_ASSERT_HINT(m_type == RAMB36E2, "Invalid width for bram type!");
			HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available in simple dual port mode!");
			HCL_ASSERT_HINT(portA, "In SDP mode, only port A can read!");
			result = pack(std::array<UInt, 4>{
				getOutputUInt(OUT_DOUT_A_DOUT), 
				getOutputUInt(OUT_DOUT_B_DOUT), 
				getOutputUInt(OUT_DOUTP_A_DOUTP),
				getOutputUInt(OUT_DOUTP_B_DOUTP),
			});
		break;
		case 36:
			if (m_type == RAMB36E2) {
				result = pack(std::array<UInt, 2>{getOutputUInt(portA?OUT_DOUT_A_DOUT:OUT_DOUT_B_DOUT), getOutputUInt(portA?OUT_DOUTP_A_DOUTP:OUT_DOUTP_B_DOUTP)});
			} else {
				HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available for RAMB36E2 or in simple dual port mode RAMB18E2!");
				HCL_ASSERT_HINT(portA, "In SDP mode, only port A can read!");
				result = pack(std::array<UInt, 4>{
					getOutputUInt(OUT_DOUT_A_DOUT), 
					getOutputUInt(OUT_DOUT_B_DOUT), 
					getOutputUInt(OUT_DOUTP_A_DOUTP),
					getOutputUInt(OUT_DOUTP_B_DOUTP),
				});
			}
		break;
		case 18:
			result = pack(std::array<UInt, 2>{getOutputUInt(portA?OUT_DOUT_A_DOUT:OUT_DOUT_B_DOUT)(0, 16_b), getOutputUInt(portA?OUT_DOUTP_A_DOUTP:OUT_DOUTP_B_DOUTP)(0, 2_b)});
		break;
		case 9:
			result = pack(std::array<UInt, 2>{getOutputUInt(portA?OUT_DOUT_A_DOUT:OUT_DOUT_B_DOUT)(0, 8_b), getOutputUInt(portA?OUT_DOUTP_A_DOUTP:OUT_DOUTP_B_DOUTP)(0, 1_b)});
		break;
		case 4:
		case 2:
		case 1:
			result = getOutputUInt(portA ? OUT_DOUT_A_DOUT : OUT_DOUT_B_DOUT)(0, BitWidth{ width });
		break;
		default:
			HCL_ASSERT_HINT(false, "Invalid width for bram type!");
		break;
	}
	if (m_type == RAMB18E2) {
		if (portA)
			result.setName("RAMB18E2_rdData_portA");
		else
			result.setName("RAMB18E2_rdData_portB");
	} else {
		if (portA)
			result.setName("RAMB36E2_rdData_portA");
		else
			result.setName("RAMB36E2_rdData_portB");
	}

	return result;
}

void RAMBxE2::connectWriteData(const UInt &input, bool portA)
{
	BitWidth dPortWidth = 32_b;
	BitWidth pPortWidth = 4_b;
	if (m_type == RAMB18E2) {
		dPortWidth = 16_b;
		pPortWidth = 2_b;
	}

	switch (input.size()) {
		case 72:
			HCL_ASSERT_HINT(m_type == RAMB36E2, "Invalid width for bram type!");
			HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available in simple dual port mode!");
			HCL_ASSERT_HINT(!portA, "In SDP mode, only port B can write!");

			connectInput(IN_DIN_A_DIN, input(0, 32_b));
			connectInput(IN_DIN_B_DIN, input(32, 32_b));
			connectInput(IN_DINP_A_DINP, input(64, 4_b));
			connectInput(IN_DINP_B_DINP, input(68, 4_b));
		break;
		case 36:
			if (m_type == RAMB36E2) {
				connectInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, input(0, 32_b));
				connectInput(portA?IN_DINP_A_DINP:IN_DINP_B_DINP, input(32, 4_b));
			} else {
				HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available for RAMB36E2 or in simple dual port mode RAMB18E2!");
				HCL_ASSERT_HINT(!portA, "In SDP mode, only port B can write!");
				connectInput(IN_DIN_A_DIN, input(0, 16_b));
				connectInput(IN_DIN_B_DIN, input(16, 16_b));
				connectInput(IN_DINP_A_DINP, input(32, 2_b));
				connectInput(IN_DINP_B_DINP, input(34, 2_b));
			}
		break;
		case 18:
			connectInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, zext(input(0, 16_b), dPortWidth));
			connectInput(portA?IN_DINP_A_DINP:IN_DINP_B_DINP, zext(input(16, 2_b), pPortWidth));
		break;
		case 9:
			connectInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, zext(input(0, 8_b), dPortWidth));
			connectInput(portA?IN_DINP_A_DINP:IN_DINP_B_DINP, zext(input(8, 1_b), pPortWidth));
		break;
		case 4:
		case 2:
		case 1:
			connectInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, zext(input, dPortWidth));
		break;
		default:
			HCL_ASSERT_HINT(false, "Invalid width for bram type!");
		break;
	}
}

void RAMBxE2::connectAddress(const UInt &input, bool portA)
{
	size_t lowerZeros = 0;

	size_t width = std::max(m_portSetups[portA?0:1].writeWidth, m_portSetups[portA?0:1].readWidth);

	switch (width) {
		case 72:
			HCL_ASSERT_HINT(m_type == RAMB36E2, "Invalid width for bram type!");
			HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available in simple dual port mode!");
			lowerZeros = 6;
		break;
		case 36:
			if (m_type == RAMB36E2) {
				lowerZeros = 5;
			} else {
				HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available for RAMB36E2 or in simple dual port mode RAMB18E2!");
				lowerZeros = 5;
			}
		break;
		case 18:
			lowerZeros = 4;
		break;
		case 9:
			lowerZeros = 3;
		break;
		case 4:
			lowerZeros = 2;
		break;
		case 2:
			lowerZeros = 1;
		break;
		case 1:
			lowerZeros = 0;
		break;
		default:
			HCL_ASSERT_HINT(false, "Invalid width for bram type!");
		break;
	}

	BitWidth totalAddrBits = m_type == RAMB36E2 ? 15_b : 14_b;

	UInt properAddr = ConstUInt(0, totalAddrBits);
	properAddr(lowerZeros, input.width()) = input;

	connectInput(portA?IN_ADDR_A_RDADDR:IN_ADDR_B_WRADDR, properAddr);
}



std::string RAMBxE2::getTypeName() const
{
	return m_name;
}

void RAMBxE2::assertValidity() const
{
}

std::string RAMBxE2::getInputName(size_t idx) const
{
	switch (idx) {
		// 18k
		case IN_ADDREN_A: return "ADDRENA";
		case IN_ADDREN_B: return "ADDRENB";
		case IN_CAS_DIMUX_A: return "CASDIMUXA";
		case IN_CAS_DIMUX_B: return "CASDIMUXB";
		case IN_CAS_DOMUX_A: return "CASDOMUXA";
		case IN_CAS_DOMUX_B: return "CASDOMUXB";
		case IN_CAS_DOMUXEN_A: return "CASDOMUXEN_A";
		case IN_CAS_DOMUXEN_B: return "CASDOMUXEN_B";
		case IN_EN_A_RD_EN: return "ENARDEN";
		case IN_EN_B_WR_EN: return "ENBWREN";
		case IN_REG_CE_A_REG_CE: return "REGCEAREGCE";
		case IN_REG_CE_B: return "REGCEB";
		case IN_SLEEP: return "SLEEP";
		case IN_ADDR_A_RDADDR: return "ADDRARDADDR";
		case IN_ADDR_B_WRADDR: return "ADDRBWRADDR";
		case IN_CAS_DIN_A: return "CASDINA";
		case IN_CAS_DIN_B: return "CASDINB";
		case IN_CAS_DINP_A: return "CASDINPA";
		case IN_CAS_DINP_B: return "CASDINPB";
		case IN_CAS_OREG_IMUX_A: return "CASOREGIMUXA";
		case IN_CAS_OREG_IMUX_B: return "CASOREGIMUXB";
		case IN_CAS_OREG_IMUXEN_A: return "CASOREGIMUXEN_A";
		case IN_CAS_OREG_IMUXEN_B: return "CASOREGIMUXEN_B";
		case IN_DIN_A_DIN: return "DINADIN";
		case IN_DIN_B_DIN: return "DINBDIN";
		case IN_DINP_A_DINP: return "DINPADINP";
		case IN_DINP_B_DINP: return "DINPBDINP";
		case IN_WE_A: return "WEA";
		case IN_WE_B_WE: return "WEBWE";
		case IN_RST_RAM_A_RST_RAM: return "RSTRAMARSTRAM";
		case IN_RST_RAM_B: return "RSTRAMB";
		case IN_RST_REG_A_RST_REG: return "RSTREGARSTREG";
		case IN_RST_REG_B: return "RSTREGB";

		// 36k
		case IN_CAS_IND_BITERR: return "CASINDBITERR";
		case IN_CAS_INS_BITERR: return "CASINSBITERR";
		case IN_ECC_PIPE_CE: return "ECCPIPECE";
		case IN_INJECT_D_BITERR: return "INJECTDBITERR";
		case IN_INJECT_S_BITERR : return "INJECTSBITERR";
		default: return "";
	}
}

std::string RAMBxE2::getOutputName(size_t idx) const
{
	switch (idx) {
		// 18k
		case OUT_CAS_DOUT_A: return "CASDOUTA";
		case OUT_CAS_DOUT_B: return "CASDOUTB";
		case OUT_CAS_DOUTP_A: return "CASDOUTPA";
		case OUT_CAS_DOUTP_B: return "CASDOUTPB";
		case OUT_DOUT_A_DOUT: return "DOUTADOUT";
		case OUT_DOUT_B_DOUT: return "DOUTBDOUT";
		case OUT_DOUTP_A_DOUTP: return "DOUTPADOUTP";
		case OUT_DOUTP_B_DOUTP: return "DOUTPBDOUTP";
		// 36k
		case OUT_CAS_OUTD_BITERR: return "CASOUTDBITERR";
		case OUT_CAS_OUTS_BITERR: return "CASOUTSBITERR";
		case OUT_D_BITERR: return "DBITERR";
		case OUT_ECC_PARITY: return "ECCPARITY";
		case OUT_RD_ADDR_ECC: return "RDADDRECC";
		case OUT_S_BITERR: return "SBITERR";
		default: return "";
	}
}

std::unique_ptr<hlim::BaseNode> RAMBxE2::cloneUnconnected() const
{
	RAMBxE2 *ptr;
	std::unique_ptr<BaseNode> res(ptr = new RAMBxE2(m_type));
	copyBaseToClone(res.get());

	return res;
}

std::string RAMBxE2::attemptInferOutputName(size_t outputPort) const
{
	return m_name + '_' + getOutputName(outputPort);
}




}
