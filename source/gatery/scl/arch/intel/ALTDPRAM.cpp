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

#include "ALTDPRAM.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <boost/format.hpp>
#include <set>

namespace gtry::scl::arch::intel {

ALTDPRAM::ALTDPRAM(size_t width, size_t depth)
{
    m_libraryName = "altera_mf";
	m_packageName = "altera_mf_components";
    m_name = "altdpram";
	m_isEntity = false;
    m_clockNames = {"", ""};
    m_resetNames = {"", ""};
	m_clocks.resize(CLK_COUNT);

	m_width = width;
	m_depth = depth;

	m_genericParameters["width"] = std::to_string(width);
	m_genericParameters["numwords"] = std::to_string(depth);

	m_genericParameters["WIDTHAD"] = std::to_string(utils::Log2C(depth));

    resizeInputs(IN_COUNT);
    resizeOutputs(OUT_COUNT);

	setOutputConnectionType(OUT_Q, {.interpretation = hlim::ConnectionType::BITVEC, .width=width});
}

std::string ALTDPRAM::RDWBehavior2Str(RDWBehavior rdw)
{
	switch (rdw) {
		case RDWBehavior::DONT_CARE:
			return "\"DONT_CARE\"";
		case RDWBehavior::CONSTRAINED_DONT_CARE:
			return "\"CONSTRAINED_DONT_CARE\"";
		case RDWBehavior::OLD_DATA:
			return "\"OLD_DATA\"";
		case RDWBehavior::NEW_DATA_MASKED_UNDEFINED:
			return "\"NEW_DATA_NO_NBE_READ\"";
	}
	HCL_ASSERT_HINT(false, "Unhandled case!");
}

ALTDPRAM &ALTDPRAM::setupReadPort(PortSetup portSetup)
{
	if (portSetup.inputRegs) {
		m_genericParameters["RDADDRESS_REG"] = "\"OUTCLOCK\"";
		m_genericParameters["RDCONTROL_REG"] = "\"OUTCLOCK\"";
	} else {
		m_genericParameters["RDADDRESS_REG"] = "\"UNREGISTERED\"";
		m_genericParameters["RDCONTROL_REG"] = "\"UNREGISTERED\"";
	}

	if (portSetup.outputRegs) 
		m_genericParameters["OUTDATA_REG"] = "\"OUTCLOCK\"";
	else
		m_genericParameters["OUTDATA_REG"] = "\"UNREGISTERED\"";

	if (portSetup.outReset) 
		m_genericParameters["OUTDATA_ACLR"] = "\"ON\"";
	else
		m_genericParameters["OUTDATA_ACLR"] = "\"OFF\"";

	if (portSetup.resetAddr)
		m_genericParameters["RDADDRESS_ACLR"] = "\"ON\"";
	else
		m_genericParameters["RDADDRESS_ACLR"] = "\"OFF\"";

	if (portSetup.resetRdEnable)
		m_genericParameters["RDCONTROL_ACLR"] = "\"ON\"";
	else
		m_genericParameters["RDCONTROL_ACLR"] = "\"OFF\"";

	if (portSetup.inputRegs || portSetup.outputRegs)
		m_clockNames[1] = "outclock";

	if (portSetup.resetAddr || portSetup.resetRdEnable || portSetup.outReset)
		m_resetNames[1] = "aclr";

	return *this;
}

ALTDPRAM &ALTDPRAM::setupWritePort(PortSetup portSetup)
{
	if (portSetup.inputRegs) {
		m_genericParameters["WRCONTROL_REG"] 				= "\"INCLOCK\"";
		m_genericParameters["WRADDRESS_REG"] 				= "\"INCLOCK\"";
		m_genericParameters["INDATA_REG"] 					= "\"INCLOCK\"";
	} else {
		m_genericParameters["WRCONTROL_REG"] 				= "\"UNREGISTERED\"";
		m_genericParameters["WRADDRESS_REG"] 				= "\"UNREGISTERED\"";
		m_genericParameters["INDATA_REG"] 					= "\"UNREGISTERED\"";
	}

	if (portSetup.resetAddr)
		m_genericParameters["WRADDRESS_ACLR"] = "\"ON\"";
	else
		m_genericParameters["WRADDRESS_ACLR"] = "\"OFF\"";

	if (portSetup.resetWrEn)
		m_genericParameters["WRCONTROL_ACLR"] = "\"ON\"";
	else
		m_genericParameters["WRCONTROL_ACLR"] = "\"OFF\"";

	if (portSetup.resetWrData)
		m_genericParameters["INDATA_ACLR"] = "\"ON\"";
	else
		m_genericParameters["INDATA_ACLR"] = "\"OFF\"";

	if (portSetup.inputRegs)
		m_clockNames[0] = "inclock";

	if (portSetup.resetAddr || portSetup.resetWrEn || portSetup.resetWrData)
		m_resetNames[0] = "aclr";

	return *this;
}


ALTDPRAM &ALTDPRAM::setupRamType(const std::string &type)
{
	//m_genericParameters["ram_block_type"] = std::move(type);
	m_genericParameters["ram_block_type"] = std::string("\"") + type + '"';
	return *this;
}

ALTDPRAM &ALTDPRAM::setupSimulationDeviceFamily(const std::string &devFamily)
{
	//m_genericParameters["ram_block_type"] = std::move(type);
	m_genericParameters["intended_device_family"] = std::string("\"") + devFamily + '"';
	return *this;
}


ALTDPRAM &ALTDPRAM::setupMixedPortRdw(RDWBehavior rdw)
{
	switch (rdw) {
		case RDWBehavior::DONT_CARE:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "\"DONT_CARE\"";
		break;
		case RDWBehavior::CONSTRAINED_DONT_CARE:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "\"CONSTRAINED_DONT_CARE\"";
		break;
		case RDWBehavior::OLD_DATA:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "\"OLD_DATA\"";
		break;
		case RDWBehavior::NEW_DATA_MASKED_UNDEFINED:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "\"NEW_DATA\"";
		break;
	}
	return *this;
}

void ALTDPRAM::connectInput(Inputs input, const Bit &bit)
{
	switch (input) {
		case IN_RDADDRESSSTALL:
		case IN_WRADDRESSSTALL:
		case IN_WREN:
		case IN_INCLOCKEN:
		case IN_RDEN:
		case IN_OUTCLOCKEN:
		case IN_ACLR:
			NodeIO::connectInput(input, bit.getReadPort());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bit to UInt input of ALTDPRAM!");
	}
}

void ALTDPRAM::connectInput(Inputs input, const UInt &UInt)
{
	switch (input) {
		case IN_DATA:
			HCL_DESIGNCHECK_HINT(UInt.size() == m_width, "Data input UInt to ALTDPRAM has different width than previously specified!");
			NodeIO::connectInput(input, UInt.getReadPort());
			trySetByteSize();
		break;
		case IN_RDADDRESS:
			NodeIO::connectInput(input, UInt.getReadPort());
			HCL_DESIGNCHECK_HINT((1ull << UInt.size()) == m_depth, "RD-Address input UInt to ALTDPRAM has different width than previously specified!");
		break;
		case IN_WRADDRESS:
			NodeIO::connectInput(input, UInt.getReadPort());
			HCL_DESIGNCHECK_HINT((1ull << UInt.size()) == m_depth, "WR-Address input UInt to ALTDPRAM has different width than previously specified!");
		break;
		case IN_BYTEENA:
			NodeIO::connectInput(input, UInt.getReadPort());
			m_genericParameters["WIDTH_BYTEENA"] = std::to_string(UInt.size());
			trySetByteSize();
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect UInt to bit input of ALTDPRAM!");
	}
}

void ALTDPRAM::trySetByteSize()
{
	auto data = getNonSignalDriver(IN_DATA);
	auto byteEn = getNonSignalDriver(IN_BYTEENA);
	if (data.node != nullptr && byteEn.node != nullptr) {

		size_t dataWidth = getOutputWidth(data);
		size_t byteEnWidth = getOutputWidth(byteEn);
		HCL_ASSERT(dataWidth % byteEnWidth == 0);

		size_t wordSize = dataWidth / byteEnWidth;

		std::set<size_t> validWordSizes = {5, 8, 9, 10};
		HCL_ASSERT(validWordSizes.contains(wordSize));

		m_genericParameters["BYTE_SIZE"] = std::to_string(wordSize);
	}
}

UInt ALTDPRAM::getOutputUInt(Outputs output)
{
	return SignalReadPort(hlim::NodePort{this, (size_t) output});
}


std::string ALTDPRAM::getTypeName() const
{
    return "ALTDPRAM";
}

void ALTDPRAM::assertValidity() const
{
}

std::string ALTDPRAM::getInputName(size_t idx) const
{
    switch (idx) {
		case IN_RDADDRESSSTALL: return "rdaddress";
		case IN_WRADDRESSSTALL: return "wraddress";
		case IN_WREN: return "wren";
		case IN_INCLOCKEN: return "inclocken";
		case IN_RDEN: return "rden";
		case IN_OUTCLOCKEN: return "outclocken";
		case IN_ACLR: return "aclr";
		case IN_DATA: return "data";
		case IN_RDADDRESS: return "rdaddress";
		case IN_WRADDRESS: return "wraddress";
		case IN_BYTEENA: return "byteena";
		default: return "";
	}
}

std::string ALTDPRAM::getOutputName(size_t idx) const
{
    switch (idx) {
		case OUT_Q: return "q";
		default: return "";
	}
}

std::unique_ptr<hlim::BaseNode> ALTDPRAM::cloneUnconnected() const
{
    ALTDPRAM *ptr;
    std::unique_ptr<BaseNode> res(ptr = new ALTDPRAM(m_width, m_depth));
    copyBaseToClone(res.get());

    return res;
}

std::string ALTDPRAM::attemptInferOutputName(size_t outputPort) const
{
	return "altdpram_" + getOutputName(outputPort);
}



}
