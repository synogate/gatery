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

#include "ALTSYNCRAM.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::intel {

ALTSYNCRAM::ALTSYNCRAM(size_t size)
{
    m_libraryName = "altera_mf";
	m_packageName = "altera_mf_components";
    m_name = "altsyncram";
	m_isEntity = false;
    m_clockNames = {"clock0", ""};
    m_resetNames = {"", ""};
	m_clocks.resize(CLK_COUNT);

	m_size = size;

    resizeInputs(IN_COUNT);
    resizeOutputs(OUT_COUNT);

	setOutputConnectionType(OUT_Q_A, {.interpretation = hlim::ConnectionType::BITVEC, .width=0}); // for now
	setOutputConnectionType(OUT_Q_B, {.interpretation = hlim::ConnectionType::BITVEC, .width=0}); // for now
}

std::string ALTSYNCRAM::RDWBehavior2Str(RDWBehavior rdw)
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

ALTSYNCRAM &ALTSYNCRAM::setupPortA(size_t width, PortSetup portSetup)
{
	setOutputConnectionType(OUT_Q_A, {.interpretation = hlim::ConnectionType::BITVEC, .width=width});
	m_genericParameters["width_a"] = std::to_string(width);
	m_genericParameters["numwords_a"] = std::to_string(m_size / width);
	m_genericParameters["read_during_write_mode_port_a"] = RDWBehavior2Str(portSetup.rdw);

	HCL_ASSERT(portSetup.inputRegs);

	if (portSetup.outputRegs)
		m_genericParameters["outdata_reg_a"] = "\"CLOCK0\"";
	else
		m_genericParameters["outdata_reg_a"] = "\"UNREGISTERED\"";

	if (portSetup.resetAddr)
		m_genericParameters["address_aclr_a"] = "\"CLEAR0\"";
	else
		m_genericParameters["address_aclr_a"] = "\"NONE\"";

	if (portSetup.resetWrEn)
		m_genericParameters["wrcontrol_aclr_a"] = "\"CLEAR0\"";
	else
		m_genericParameters["wrcontrol_aclr_a"] = "\"NONE\"";

	if (portSetup.outReset)
		m_genericParameters["outdata_aclr_a"] = "\"CLEAR0\"";
	else
		m_genericParameters["outdata_aclr_a"] = "\"NONE\"";

	if (portSetup.resetAddr || portSetup.resetWrEn || portSetup.outReset)
		m_resetNames[0] = "aclr0";


	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupPortB(size_t width, PortSetup portSetup)
{
	setOutputConnectionType(OUT_Q_B, {.interpretation = hlim::ConnectionType::BITVEC, .width=width});
	m_genericParameters["width_b"] = std::to_string(width);
	m_genericParameters["numwords_b"] = std::to_string(m_size / width);
	m_genericParameters["read_during_write_mode_port_b"] = RDWBehavior2Str(portSetup.rdw);


	if ((portSetup.inputRegs || portSetup.outputRegs) && portSetup.dualClock) {
		m_clockNames[1] = "clock1";
	}

	if (portSetup.inputRegs) {
		if (portSetup.dualClock) {
			m_genericParameters["rdcontrol_reg_b"] 				= "\"CLOCK1\"";
			m_genericParameters["address_reg_b"] 				= "\"CLOCK1\"";
			m_genericParameters["indata_reg_b"] 				= "\"CLOCK1\"";
			m_genericParameters["wrcontrol_wraddress_reg_b"]	= "\"CLOCK1\"";
			m_genericParameters["byteena_reg_b"] 				= "\"CLOCK1\"";
		} else {
			m_genericParameters["rdcontrol_reg_b"] 				= "\"CLOCK0\"";
			m_genericParameters["address_reg_b"] 				= "\"CLOCK0\"";
			m_genericParameters["indata_reg_b"] 				= "\"CLOCK0\"";
			m_genericParameters["wrcontrol_wraddress_reg_b"] 	= "\"CLOCK0\"";
			m_genericParameters["byteena_reg_b"]				= "\"CLOCK0\"";
		}
	} else {
			m_genericParameters["rdcontrol_reg_b"] 				= "\"UNREGISTERED\"";
			m_genericParameters["address_reg_b"] 				= "\"UNREGISTERED\"";
			m_genericParameters["indata_reg_b"] 				= "\"UNREGISTERED\"";
			m_genericParameters["wrcontrol_wraddress_reg_b"] 	= "\"UNREGISTERED\"";
			m_genericParameters["byteena_reg_b"]				= "\"UNREGISTERED\"";
	}

	if (portSetup.outputRegs)
		if (portSetup.dualClock)
			m_genericParameters["outdata_reg_b"] = "\"CLOCK1\"";
		else
			m_genericParameters["outdata_reg_b"] = "\"CLOCK0\"";
	else
		m_genericParameters["outdata_reg_b"] = "\"UNREGISTERED\"";

	if (portSetup.resetAddr)
		m_genericParameters["address_aclr_b"] = "\"CLEAR0\"";
	else
		m_genericParameters["address_aclr_b"] = "\"NONE\"";

	if (portSetup.resetWrEn)
		m_genericParameters["wrcontrol_aclr_b"] = "\"CLEAR0\"";
	else
		m_genericParameters["wrcontrol_aclr_b"] = "\"NONE\"";

	if (portSetup.outReset)
		m_genericParameters["outdata_aclr_b"] = "\"CLEAR0\"";
	else
		m_genericParameters["outdata_aclr_b"] = "\"NONE\"";

	if (portSetup.resetAddr || portSetup.resetWrEn || portSetup.outReset) {
		if (portSetup.dualClock)
			m_resetNames[1] = "aclr1";
		else
			m_resetNames[0] = "aclr0";
	}

	return *this;
}



ALTSYNCRAM &ALTSYNCRAM::setupSinglePort()
{
	m_genericParameters["operation_mode"] = "\"SINGLE_PORT\"";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupSimpleDualPort()
{
	m_genericParameters["operation_mode"] = "\"DUAL_PORT\"";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupTrueDualPort()
{
	m_genericParameters["operation_mode"] = "\"BIDIR_DUAL_PORT\"";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupROM()
{
	m_genericParameters["operation_mode"] = "\"ROM\"";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupRamType(const std::string &type)
{
	//m_genericParameters["ram_block_type"] = std::move(type);
	m_genericParameters["ram_block_type"] = std::string("\"") + type + '"';
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupSimulationDeviceFamily(const std::string &devFamily)
{
	//m_genericParameters["ram_block_type"] = std::move(type);
	m_genericParameters["intended_device_family"] = std::string("\"") + devFamily + '"';
	return *this;
}


ALTSYNCRAM &ALTSYNCRAM::setupMixedPortRdw(RDWBehavior rdw)
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

void ALTSYNCRAM::connectInput(Inputs input, const Bit &bit)
{
	switch (input) {
		case IN_WREN_A:
		case IN_RDEN_A:
		case IN_WREN_B:
		case IN_RDEN_B:
		case IN_CLOCKEN_0:
		case IN_CLOCKEN_1:
		case IN_ACLR_0:
		case IN_ACLR_1:
			NodeIO::connectInput(input, bit.getReadPort());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bit to bvec input of ALTSYNCRAM!");
	}
}

void ALTSYNCRAM::connectInput(Inputs input, const BVec &bvec)
{
	size_t sizeA = getOutputConnectionType(OUT_Q_A).width;
	size_t sizeB = getOutputConnectionType(OUT_Q_B).width;

	switch (input) {
		case IN_DATA_A:
			HCL_DESIGNCHECK_HINT(bvec.size() == sizeA, "Data input bvec to ALTSYNCRAM has different width than previously specified!");
			NodeIO::connectInput(input, bvec.getReadPort());			
		break;
		case IN_ADDRESS_A:
			NodeIO::connectInput(input, bvec.getReadPort());
			m_genericParameters["widthad_a"] = std::to_string(bvec.size());
		break;
		case IN_BYTEENA_A:
			NodeIO::connectInput(input, bvec.getReadPort());
			m_genericParameters["width_byteena_a"] = std::to_string(bvec.size());
		break;
		case IN_DATA_B:
			HCL_DESIGNCHECK_HINT(bvec.size() == sizeB, "Data input bvec to ALTSYNCRAM has different width than previously specified!");
			NodeIO::connectInput(input, bvec.getReadPort());			
		break;
		case IN_ADDRESS_B:
			NodeIO::connectInput(input, bvec.getReadPort());
			m_genericParameters["widthad_b"] = std::to_string(bvec.size());
		break;
		case IN_BYTEENA_B:
			NodeIO::connectInput(input, bvec.getReadPort());
			m_genericParameters["width_byteena_b"] = std::to_string(bvec.size());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bvec to bit input of ALTSYNCRAM!");
	}
}

BVec ALTSYNCRAM::getOutputBVec(Outputs output)
{
	return SignalReadPort(hlim::NodePort(this, output));
}


std::string ALTSYNCRAM::getTypeName() const
{
    return "ALTSYNCRAM";
}

void ALTSYNCRAM::assertValidity() const
{
}

std::string ALTSYNCRAM::getInputName(size_t idx) const
{
    switch (idx) {
		case IN_WREN_A: return "wren_a";
		case IN_RDEN_A: return "rden_a";
		case IN_WREN_B: return "wren_b";
		case IN_RDEN_B: return "rden_b";
		case IN_CLOCKEN_0: return "clocken0";
		case IN_CLOCKEN_1: return "clocken1";
		case IN_ACLR_0: return "aclr0";
		case IN_ACLR_1: return "aclr1";
		case IN_DATA_A: return "data_a";
		case IN_ADDRESS_A: return "address_a";
		case IN_BYTEENA_A: return "byteena_a";
		case IN_DATA_B: return "data_b";
		case IN_ADDRESS_B: return "address_b";
		case IN_BYTEENA_B: return "byteena_b";
		default: return "";
	}
}

std::string ALTSYNCRAM::getOutputName(size_t idx) const
{
    switch (idx) {
		case OUT_Q_A: return "q_a";
		case OUT_Q_B: return "q_b";
		default: return "";
	}
}

std::unique_ptr<hlim::BaseNode> ALTSYNCRAM::cloneUnconnected() const
{
    ALTSYNCRAM *ptr;
    std::unique_ptr<BaseNode> res(ptr = new ALTSYNCRAM(m_size));
    copyBaseToClone(res.get());

    return res;
}

std::string ALTSYNCRAM::attemptInferOutputName(size_t outputPort) const
{
	return "altsyncram_" + getOutputName(outputPort);
}



}
