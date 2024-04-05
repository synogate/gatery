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
#include "gatery/scl_pch.h"

#include "RAMBxE2.h"

#include <gatery/frontend/Constant.h>
#include <gatery/frontend/Pack.h>
#include <gatery/frontend/Clock.h>
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


	size_t mult = 1;
	size_t add = 0;
	if (m_type == RAMB36E2) {
		mult = 2;
		add = 1;
		resizeIOPorts(IN_COUNT_36, OUT_COUNT_36);
	} else {
		resizeIOPorts(IN_COUNT_18, OUT_COUNT_18);
	}
	declInputBitVector(IN_ADDR_A_RDADDR, "ADDRARDADDR", 14+add);
	declInputBitVector(IN_ADDR_B_WRADDR, "ADDRBWRADDR", 14+add);
	declInputBit(IN_ADDREN_A, "ADDRENA");
	declInputBit(IN_ADDREN_B, "ADDRENB");
	declInputBit(IN_CAS_DIMUX_A, "CASDIMUXA");
	declInputBit(IN_CAS_DIMUX_B, "CASDIMUXB");
	declInputBitVector(IN_CAS_DIN_A, "CASDINA", 16*mult);
	declInputBitVector(IN_CAS_DIN_B, "CASDINB", 16*mult);
	declInputBitVector(IN_CAS_DINP_A, "CASDINPA", 2*mult);
	declInputBitVector(IN_CAS_DINP_B, "CASDINPB", 2*mult);
	declInputBit(IN_CAS_DOMUX_A, "CASDOMUXA");
	declInputBit(IN_CAS_DOMUX_B, "CASDOMUXB");
	declInputBit(IN_CAS_DOMUXEN_A, "CASDOMUXEN_A");
	declInputBit(IN_CAS_DOMUXEN_B, "CASDOMUXEN_B");
	declInputBit(IN_CAS_OREG_IMUX_A, "CASOREGIMUXA");
	declInputBit(IN_CAS_OREG_IMUX_B, "CASOREGIMUXB");
	declInputBit(IN_CAS_OREG_IMUXEN_A, "CASOREGIMUXEN_A");
	declInputBit(IN_CAS_OREG_IMUXEN_B, "CASOREGIMUXEN_B");
	declInputBitVector(IN_DIN_A_DIN, "DINADIN", 16*mult);
	declInputBitVector(IN_DIN_B_DIN, "DINBDIN", 16*mult);
	declInputBitVector(IN_DINP_A_DINP, "DINPADINP", 2*mult);
	declInputBitVector(IN_DINP_B_DINP, "DINPBDINP", 2*mult);
	declInputBit(IN_EN_A_RD_EN, "ENARDEN");
	declInputBit(IN_EN_B_WR_EN, "ENBWREN");
	declInputBit(IN_REG_CE_A_REG_CE, "REGCEAREGCE");
	declInputBit(IN_REG_CE_B, "REGCEB");
	declInputBit(IN_RST_RAM_A_RST_RAM, "RSTRAMARSTRAM");
	declInputBit(IN_RST_RAM_B, "RSTRAMB");
	declInputBit(IN_RST_REG_A_RST_REG, "RSTREGARSTREG");
	declInputBit(IN_RST_REG_B, "RSTREGB");
	declInputBit(IN_SLEEP, "SLEEP");
	declInputBitVector(IN_WE_A, "WEA", 2*mult);
	declInputBitVector(IN_WE_B_WE, "WEBWE", 4*mult);
			
			
	// 36k
	if (m_type == RAMB36E2) {
		declInputBit(IN_CAS_IND_BITERR, "CASINDBITERR");
		declInputBit(IN_CAS_INS_BITERR, "CASINSBITERR");
		declInputBit(IN_ECC_PIPE_CE, "ECCPIPECE");
		declInputBit(IN_INJECT_D_BITERR, "INJECTDBITERR");
		declInputBit(IN_INJECT_S_BITERR, "INJECTSBITERR");
	}


	declOutputBitVector(OUT_CAS_DOUT_A, "CASDOUTA", 16*mult);
	declOutputBitVector(OUT_CAS_DOUT_B, "CASDOUTB", 16*mult);
	declOutputBitVector(OUT_CAS_DOUTP_A, "CASDOUTPA", 2*mult);
	declOutputBitVector(OUT_CAS_DOUTP_B, "CASDOUTPB", 2*mult);
	declOutputBitVector(OUT_DOUT_A_DOUT, "DOUTADOUT", 16*mult);
	declOutputBitVector(OUT_DOUT_B_DOUT, "DOUTBDOUT", 16*mult);
	declOutputBitVector(OUT_DOUTP_A_DOUTP, "DOUTPADOUTP", 2*mult);
	declOutputBitVector(OUT_DOUTP_B_DOUTP, "DOUTPBDOUTP", 2*mult);

	// 36k
	if (m_type == RAMB36E2) {
		declOutputBit(OUT_CAS_OUTD_BITERR, "CASOUTDBITERR");
		declOutputBit(OUT_CAS_OUTS_BITERR, "CASOUTSBITERR");
		declOutputBit(OUT_RD_ADDR_ECC, "RDADDRECC");
		declOutputBit(OUT_S_BITERR, "SBITERR");
		declOutputBitVector(OUT_ECC_PARITY, "ECCPARITY", 8);
		declOutputBitVector(OUT_RD_ADDR_ECC, "RDADDRECC", 9);
	}


	m_genericParameters["SIM_COLLISION_CHECK"] = "GENERATE_X_ONLY";
}

void RAMBxE2::defaultInputs(bool writePortA, bool writePortB)
{
	BVec undefinedData;
	BVec undefinedParity;
	BVec undefinedAddr;

	if (m_type == RAMB18E2) {
		undefinedData = ConstBVec(BitWidth(16));
		undefinedParity = ConstBVec(BitWidth(2));
		undefinedAddr = ConstBVec(BitWidth(14));
	} else {
		undefinedData = ConstBVec(BitWidth(32));
		undefinedParity = ConstBVec(BitWidth(4));
		undefinedAddr = ConstBVec(BitWidth(15));
	}

	Bit oneB = '1';
	Bit zeroB = '0';
	Bit undefB = 'x';

	setInput(IN_ADDREN_A, oneB);
	setInput(IN_ADDREN_B, oneB);

	setInput(IN_CAS_DIMUX_A, zeroB);
	setInput(IN_CAS_DIMUX_B, zeroB);

	setInput(IN_CAS_DIN_A, undefinedData);
	setInput(IN_CAS_DIN_B, undefinedData);
	setInput(IN_CAS_DINP_A, undefinedParity);
	setInput(IN_CAS_DINP_B, undefinedParity);

	setInput(IN_CAS_DOMUX_A, zeroB);
	setInput(IN_CAS_DOMUX_B, zeroB);

	setInput(IN_CAS_DOMUXEN_A, oneB);
	setInput(IN_CAS_DOMUXEN_B, oneB);

	setInput(IN_CAS_OREG_IMUX_A, zeroB);
	setInput(IN_CAS_OREG_IMUX_B, zeroB);

	setInput(IN_CAS_OREG_IMUXEN_A, oneB);
	setInput(IN_CAS_OREG_IMUXEN_B, oneB);


	setInput(IN_DIN_A_DIN, undefinedData);
	setInput(IN_DIN_B_DIN, undefinedData);
	setInput(IN_DINP_A_DINP, undefinedParity);
	setInput(IN_DINP_B_DINP, undefinedParity);

	setInput(IN_EN_A_RD_EN, zeroB);
	setInput(IN_EN_B_WR_EN, zeroB);

	setInput(IN_REG_CE_A_REG_CE, oneB);
	setInput(IN_REG_CE_B, oneB);

	setInput(IN_RST_RAM_A_RST_RAM, zeroB);
	setInput(IN_RST_RAM_B, zeroB);
	setInput(IN_RST_REG_A_RST_REG, zeroB);
	setInput(IN_RST_REG_B, zeroB);

	setInput(IN_SLEEP, zeroB);

	if (m_type == RAMB36E2) {
		if (writePortA)
			setInput(IN_WE_A, BVec("b1111"));
		else
			setInput(IN_WE_A, BVec("b0000"));

		if (writePortB)
			setInput(IN_WE_B_WE, BVec("b11111111"));
		else
			setInput(IN_WE_B_WE, BVec("b00000000"));
	} else {
		if (writePortA)
			setInput(IN_WE_A, BVec("b11"));
		else
			setInput(IN_WE_A, BVec("b00"));

		if (writePortB)
			setInput(IN_WE_B_WE, BVec("b1111"));
		else
			setInput(IN_WE_B_WE, BVec("b0000"));	
	}

	if (m_type == RAMB36E2) {
		setInput(IN_CAS_IND_BITERR, undefB);
		setInput(IN_CAS_INS_BITERR, undefB);

		setInput(IN_ECC_PIPE_CE, zeroB);
		setInput(IN_INJECT_D_BITERR, zeroB);
		setInput(IN_INJECT_S_BITERR, zeroB);
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


void RAMBxE2::setInput(size_t input, const Bit &bit)
{
	HCL_DESIGNCHECK_HINT(m_type == RAMB36E2 || input < IN_COUNT_18, "Input only available for RAMB32E2!");
	ExternalComponent::setInput(input, bit);
}

void RAMBxE2::setInput(size_t input, const BVec &bvec)
{
	HCL_DESIGNCHECK_HINT(m_type == RAMB36E2 || input < IN_COUNT_18, "Input only available for RAMB32E2!");
	ExternalComponent::setInput(input, bvec);
}

Bit RAMBxE2::getOutputBit(size_t output)
{
	HCL_DESIGNCHECK_HINT(m_type == RAMB36E2 || output < OUT_COUNT_18, "Input only available for RAMB32E2!");
	return ExternalComponent::getOutputBit(output);
}

BVec RAMBxE2::getOutputBVec(size_t output)
{
	HCL_DESIGNCHECK_HINT(m_type == RAMB36E2 || output < OUT_COUNT_18, "Input only available for RAMB32E2!");
	return ExternalComponent::getOutputBVec(output);
}


BVec RAMBxE2::getReadData(size_t width, bool portA)
{
	BVec result;

	// std::array<UInt, xx> is needed to ensure pack order doesn't reverse (we need to change that)

	switch (width) {
		case 72:
			HCL_ASSERT_HINT(m_type == RAMB36E2, "Invalid width for bram type!");
			HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available in simple dual port mode!");
			HCL_ASSERT_HINT(portA, "In SDP mode, only port A can read!");
			result = (BVec) pack(std::array<BVec, 4>{
				getOutputBVec(OUT_DOUT_A_DOUT), 
				getOutputBVec(OUT_DOUT_B_DOUT), 
				getOutputBVec(OUT_DOUTP_A_DOUTP),
				getOutputBVec(OUT_DOUTP_B_DOUTP),
			});
		break;
		case 36:
			if (m_type == RAMB36E2) {
				result = (BVec) pack(std::array<BVec, 2>{getOutputBVec(portA?OUT_DOUT_A_DOUT:OUT_DOUT_B_DOUT), getOutputBVec(portA?OUT_DOUTP_A_DOUTP:OUT_DOUTP_B_DOUTP)});
			} else {
				HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available for RAMB36E2 or in simple dual port mode RAMB18E2!");
				HCL_ASSERT_HINT(portA, "In SDP mode, only port A can read!");
				result = (BVec) pack(std::array<BVec, 4>{
					getOutputBVec(OUT_DOUT_A_DOUT), 
					getOutputBVec(OUT_DOUT_B_DOUT), 
					getOutputBVec(OUT_DOUTP_A_DOUTP),
					getOutputBVec(OUT_DOUTP_B_DOUTP),
				});
			}
		break;
		case 18:
			result = (BVec) pack(std::array<BVec, 2>{getOutputBVec(portA?OUT_DOUT_A_DOUT:OUT_DOUT_B_DOUT)(0, 16_b), getOutputBVec(portA?OUT_DOUTP_A_DOUTP:OUT_DOUTP_B_DOUTP)(0, 2_b)});
		break;
		case 9:
			result = (BVec) pack(std::array<BVec, 2>{getOutputBVec(portA?OUT_DOUT_A_DOUT:OUT_DOUT_B_DOUT)(0, 8_b), getOutputBVec(portA?OUT_DOUTP_A_DOUTP:OUT_DOUTP_B_DOUTP)(0, 1_b)});
		break;
		case 4:
		case 2:
		case 1:
			result = getOutputBVec(portA ? OUT_DOUT_A_DOUT : OUT_DOUT_B_DOUT)(0, BitWidth{ width });
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

void RAMBxE2::connectWriteData(const BVec &input, bool portA)
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

			setInput(IN_DIN_A_DIN, input(0, 32_b));
			setInput(IN_DIN_B_DIN, input(32, 32_b));
			setInput(IN_DINP_A_DINP, input(64, 4_b));
			setInput(IN_DINP_B_DINP, input(68, 4_b));
		break;
		case 36:
			if (m_type == RAMB36E2) {
				setInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, input(0, 32_b));
				setInput(portA?IN_DINP_A_DINP:IN_DINP_B_DINP, input(32, 4_b));
			} else {
				HCL_ASSERT_HINT(isSimpleDualPort() || isRom(), "Width only available for RAMB36E2 or in simple dual port mode RAMB18E2!");
				HCL_ASSERT_HINT(!portA, "In SDP mode, only port B can write!");
				setInput(IN_DIN_A_DIN, input(0, 16_b));
				setInput(IN_DIN_B_DIN, input(16, 16_b));
				setInput(IN_DINP_A_DINP, input(32, 2_b));
				setInput(IN_DINP_B_DINP, input(34, 2_b));
			}
		break;
		case 18:
			setInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, zext(input(0, 16_b), dPortWidth));
			setInput(portA?IN_DINP_A_DINP:IN_DINP_B_DINP, zext(input(16, 2_b), pPortWidth));
		break;
		case 9:
			setInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, zext(input(0, 8_b), dPortWidth));
			setInput(portA?IN_DINP_A_DINP:IN_DINP_B_DINP, zext(input(8, 1_b), pPortWidth));
		break;
		case 4:
		case 2:
		case 1:
			setInput(portA?IN_DIN_A_DIN:IN_DIN_B_DIN, zext(input, dPortWidth));
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

	setInput(portA?IN_ADDR_A_RDADDR:IN_ADDR_B_WRADDR, (BVec) properAddr);
}



std::string RAMBxE2::getTypeName() const
{
	return m_name;
}

void RAMBxE2::assertValidity() const
{
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


hlim::OutputClockRelation RAMBxE2::getOutputClockRelation(size_t output) const
{
	switch (output) {
		case OUT_CAS_DOUT_A: 
		case OUT_CAS_DOUTP_A: 
		case OUT_DOUT_A_DOUT: 
		case OUT_DOUTP_A_DOUTP: 
			return { .dependentClocks={ m_clocks[CLK_A_RD] } };

		case OUT_CAS_DOUT_B: 
		case OUT_CAS_DOUTP_B: 
		case OUT_DOUT_B_DOUT: 
		case OUT_DOUTP_B_DOUTP: 
			if (isSimpleDualPort() || isRom())
				return { .dependentClocks = { m_clocks[CLK_A_RD] } };
			else
				return { .dependentClocks = { m_clocks[CLK_B_WR] } };

		// I'm not too certain about these
		case OUT_CAS_OUTD_BITERR:
			return { .dependentInputs={ IN_CAS_IND_BITERR } };
		case OUT_CAS_OUTS_BITERR:
			return { .dependentInputs={ IN_CAS_INS_BITERR } };
		case OUT_D_BITERR: 
		case OUT_ECC_PARITY: 
		case OUT_RD_ADDR_ECC: 
		case OUT_S_BITERR: 
			return { };
	}	
	return { };
}

bool RAMBxE2::checkValidInputClocks(std::span<hlim::SignalClockDomain> inputClocks) const
{
	auto clocksCompatible = [&](const hlim::Clock *clkA, const hlim::Clock *clkB) {
		if (clkA == nullptr || clkB == nullptr) return false;
		return clkA->getClockPinSource() == clkB->getClockPinSource();
	};

	auto checkCompatibleWith = [&](size_t input, const hlim::Clock *clk) {
		if (getNonSignalDriver(input).node == nullptr) return true;

		switch (inputClocks[input].type) {
			case hlim::SignalClockDomain::UNKNOWN: return false;
			case hlim::SignalClockDomain::CONSTANT: return true;
			case hlim::SignalClockDomain::CLOCK: return clocksCompatible(inputClocks[input].clk, clk);
		}
		return false;
	};

	if (!checkCompatibleWith(IN_ADDR_A_RDADDR, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_ADDREN_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_DIMUX_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_DIN_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_DINP_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_DOMUX_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_DOMUXEN_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_OREG_IMUX_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_CAS_OREG_IMUXEN_A, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_EN_A_RD_EN, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_REG_CE_A_REG_CE, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_RST_RAM_A_RST_RAM, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_RST_REG_A_RST_REG, m_clocks[CLK_A_RD])) return false;
	if (!checkCompatibleWith(IN_WE_A, m_clocks[CLK_A_RD])) return false;

	if (!checkCompatibleWith(IN_ADDR_B_WRADDR, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_ADDREN_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_DIMUX_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_DIN_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_DINP_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_DOMUX_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_DOMUXEN_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_OREG_IMUX_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_CAS_OREG_IMUXEN_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_EN_B_WR_EN, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_REG_CE_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_RST_RAM_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_RST_REG_B, m_clocks[CLK_B_WR])) return false;
	if (!checkCompatibleWith(IN_WE_B_WE, m_clocks[CLK_B_WR])) return false;

	if (isSimpleDualPort() || isRom()) {
		if (!checkCompatibleWith(IN_DIN_A_DIN, m_clocks[CLK_B_WR])) return false;
		if (!checkCompatibleWith(IN_DINP_A_DINP, m_clocks[CLK_B_WR])) return false;
		if (!checkCompatibleWith(IN_DIN_B_DIN, m_clocks[CLK_B_WR])) return false;
		if (!checkCompatibleWith(IN_DINP_B_DINP, m_clocks[CLK_B_WR])) return false;
	} else {
		if (!checkCompatibleWith(IN_DIN_A_DIN, m_clocks[CLK_A_RD])) return false;
		if (!checkCompatibleWith(IN_DINP_A_DINP, m_clocks[CLK_A_RD])) return false;
		if (!checkCompatibleWith(IN_DIN_B_DIN, m_clocks[CLK_B_WR])) return false;
		if (!checkCompatibleWith(IN_DINP_B_DINP, m_clocks[CLK_B_WR])) return false;
	}

	return true;
}

void RAMBxE2::copyBaseToClone(BaseNode *copy) const
{
	ExternalComponent::copyBaseToClone(copy);
	auto *other = (RAMBxE2*)copy;

	other->m_memoryInitialization = m_memoryInitialization;
	other->m_type = m_type;

	other->m_portSetups[0] = m_portSetups[0];
	other->m_portSetups[1] = m_portSetups[1];
	other->m_clockDomains = m_clockDomains;
}

}
