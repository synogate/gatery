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

#include "MemoryInitializationFile.h"

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


	m_genericParameters["outdata_reg_a"] = "\"UNREGISTERED\"";
	m_genericParameters["outdata_reg_b"] = "\"UNREGISTERED\"";
	
	m_genericParameters["rdcontrol_reg_b"] 				= "\"CLOCK1\"";
	m_genericParameters["address_reg_b"] 				= "\"CLOCK1\"";
	m_genericParameters["indata_reg_b"] 				= "\"CLOCK1\"";
	m_genericParameters["wrcontrol_wraddress_reg_b"]	= "\"CLOCK1\"";
	m_genericParameters["byteena_reg_b"] 				= "\"CLOCK1\"";

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
	m_widthPortA = width;
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
		/// @todo: I think this may not be legal for altsyncram
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
			NodeIO::connectInput(input, bit.readPort());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect bit to UInt input of ALTSYNCRAM!");
	}
}

void ALTSYNCRAM::connectInput(Inputs input, const UInt &UInt)
{
	size_t sizeA = getOutputConnectionType(OUT_Q_A).width;
	size_t sizeB = getOutputConnectionType(OUT_Q_B).width;

	switch (input) {
		case IN_DATA_A:
			HCL_DESIGNCHECK_HINT(UInt.size() == sizeA, "Data input UInt to ALTSYNCRAM has different width than previously specified!");
			NodeIO::connectInput(input, UInt.readPort());			
		break;
		case IN_ADDRESS_A:
			NodeIO::connectInput(input, UInt.readPort());
			m_genericParameters["widthad_a"] = std::to_string(UInt.size());
		break;
		case IN_BYTEENA_A:
			NodeIO::connectInput(input, UInt.readPort());
			m_genericParameters["width_byteena_a"] = std::to_string(UInt.size());
		break;
		case IN_DATA_B:
			HCL_DESIGNCHECK_HINT(UInt.size() == sizeB, "Data input UInt to ALTSYNCRAM has different width than previously specified!");
			NodeIO::connectInput(input, UInt.readPort());			
		break;
		case IN_ADDRESS_B:
			NodeIO::connectInput(input, UInt.readPort());
			m_genericParameters["widthad_b"] = std::to_string(UInt.size());
		break;
		case IN_BYTEENA_B:
			NodeIO::connectInput(input, UInt.readPort());
			m_genericParameters["width_byteena_b"] = std::to_string(UInt.size());
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Trying to connect UInt to bit input of ALTSYNCRAM!");
	}
}

UInt ALTSYNCRAM::getOutputUInt(Outputs output)
{
	return SignalReadPort(hlim::NodePort{this, (size_t)output});
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

std::vector<std::string> ALTSYNCRAM::getSupportFiles() const
{
	if (m_memoryInitialization.size() != 0)
		if (sim::anyDefined(m_memoryInitialization))
			return { "memoryInitialization.mif" };

	return {};
}

void ALTSYNCRAM::setupSupportFile(size_t idx, const std::string &filename, std::ostream &stream)
{
	HCL_ASSERT(idx == 0);
	m_genericParameters["init_file"] = (boost::format("\"%s\"") % filename).str();
	m_genericParameters["init_file_layout"] = "\"PORT_A\"";

	writeMemoryInitializationFile(stream, m_widthPortA, m_memoryInitialization);
}


hlim::OutputClockRelation ALTSYNCRAM::getOutputClockRelation(size_t output) const
{
	std::string outdata_reg;
	if (output == OUT_Q_A)
		outdata_reg = m_genericParameters.find("outdata_reg_a")->second;
	else
		outdata_reg = m_genericParameters.find("outdata_reg_b")->second;

	if (outdata_reg == "\"CLOCK0\"") 
		return { .dependentClocks={ 0 } };

	if (outdata_reg == "\"CLOCK1\"") 
		return { .dependentClocks={ 1 } };


	std::string addr_reg;
	if (output == OUT_Q_A)
		addr_reg =  "\"CLOCK0\"";
	else
		addr_reg = m_genericParameters.find("address_reg_b")->second;

	if (addr_reg == "\"CLOCK0\"") 
		return { .dependentClocks={ 0 } };

	if (addr_reg == "\"CLOCK1\"") 
		return { .dependentClocks={ 1 } };

	HCL_ASSERT_HINT(false, "Inconsistent configuration of ALTSYNCRAM!");
}

bool ALTSYNCRAM::checkValidInputClocks(std::span<hlim::SignalClockDomain> inputClocks) const
{
	auto clocksCompatible = [&](const hlim::Clock *clkA, const hlim::Clock *clkB) {
		if (clkA == nullptr || clkB == nullptr) return false;
		return clkA->getClockPinSource() == clkB->getClockPinSource();
	};

	auto check = [&](size_t input, const std::string &clk) {
		switch (inputClocks[input].type) {
			case hlim::SignalClockDomain::UNKNOWN: return false;
			case hlim::SignalClockDomain::CONSTANT: return true;
			case hlim::SignalClockDomain::CLOCK: {
				if (clk == "\"CLOCK0\"") return clocksCompatible(inputClocks[input].clk, m_clocks[0]);
				if (clk == "\"CLOCK1\"") return clocksCompatible(inputClocks[input].clk, m_clocks[1]);
				HCL_ASSERT_HINT(false, "Invalid configuration of ALTSYNCRAM!");
			}
		}
	};

	if (!check(IN_WREN_A, "\"CLOCK0\"")) return false;
	if (!check(IN_RDEN_A, "\"CLOCK0\"")) return false;

	if (!check(IN_WREN_B, m_genericParameters.find("wrcontrol_wraddress_reg_b")->second)) return false;
	if (!check(IN_RDEN_B, m_genericParameters.find("rdcontrol_reg_b")->second)) return false;


	if (!check(IN_DATA_A, "\"CLOCK0\"")) return false;
	if (!check(IN_ADDRESS_A, "\"CLOCK0\"")) return false;
	if (!check(IN_BYTEENA_A, "\"CLOCK0\"")) return false;


	if (!check(IN_DATA_B, m_genericParameters.find("indata_reg_b")->second)) return false;
	if (!check(IN_ADDRESS_B, m_genericParameters.find("address_reg_b")->second)) return false;
	if (!check(IN_ADDRESS_B, m_genericParameters.find("wrcontrol_wraddress_reg_b")->second)) return false;
	if (!check(IN_BYTEENA_B, m_genericParameters.find("byteena_reg_b")->second)) return false;

	return true;
}




}
