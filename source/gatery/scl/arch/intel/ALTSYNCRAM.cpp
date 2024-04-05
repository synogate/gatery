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

#include "ALTSYNCRAM.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>
#include <gatery/hlim/Clock.h>

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


	m_genericParameters["outdata_reg_a"] = "UNREGISTERED";
	m_genericParameters["outdata_reg_b"] = "UNREGISTERED";
	
	m_genericParameters["rdcontrol_reg_b"] 				= "CLOCK1";
	m_genericParameters["address_reg_b"] 				= "CLOCK1";
	m_genericParameters["indata_reg_b"] 				= "CLOCK1";
	m_genericParameters["wrcontrol_wraddress_reg_b"]	= "CLOCK1";
	m_genericParameters["byteena_reg_b"] 				= "CLOCK1";

	m_size = size;

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_WREN_A, "WREN_A");
	declInputBit(IN_RDEN_A, "RDEN_A");
	declInputBit(IN_WREN_B, "WREN_B");
	declInputBit(IN_RDEN_B, "RDEN_B");
	declInputBit(IN_CLOCKEN_0, "CLOCKEN_0");
	declInputBit(IN_CLOCKEN_1, "CLOCKEN_1");
	declInputBit(IN_ACLR_0, "ACLR_0");
	declInputBit(IN_ACLR_1, "ACLR_1");

	declInputBitVector(IN_DATA_A, "DATA_A", 0, "width_a");
	declInputBitVector(IN_ADDRESS_A, "ADDRESS_A", 0, "widthad_a");
	declInputBitVector(IN_BYTEENA_A, "BYTEENA_A", 0, "width_byteena_a");
	declInputBitVector(IN_DATA_B, "DATA_B", 0, "width_b");
	declInputBitVector(IN_ADDRESS_B, "ADDRESS_B", 0, "widthad_b");
	declInputBitVector(IN_BYTEENA_B, "BYTEENA_B", 0, "width_byteena_b");


	declOutputBitVector(OUT_Q_A, "Q_A", 0, "width_a");
	declOutputBitVector(OUT_Q_B, "Q_B", 0, "width_b");
	setOutputType(OUT_Q_A, OUTPUT_LATCHED);
	setOutputType(OUT_Q_B, OUTPUT_LATCHED);
}

std::string ALTSYNCRAM::RDWBehavior2Str(RDWBehavior rdw)
{
	switch (rdw) {
		case RDWBehavior::DONT_CARE:
			return "DONT_CARE";
		case RDWBehavior::CONSTRAINED_DONT_CARE:
			return "CONSTRAINED_DONT_CARE";
		case RDWBehavior::OLD_DATA:
			return "OLD_DATA";
		case RDWBehavior::NEW_DATA_MASKED_UNDEFINED:
			return "NEW_DATA_NO_NBE_READ";
	}
	HCL_ASSERT_HINT(false, "Unhandled case!");
}

ALTSYNCRAM &ALTSYNCRAM::setupPortA(size_t width, PortSetup portSetup)
{
	setOutputConnectionType(OUT_Q_A, {.type = hlim::ConnectionType::BITVEC, .width=width});
	m_widthPortA = width;
	m_genericParameters["width_a"] = width;
	declInputBitVector(IN_DATA_A, "DATA_A", width, "width_a");
	declOutputBitVector(OUT_Q_A, "Q_A", width, "width_a");
	m_genericParameters["numwords_a"] = m_size / width;
	m_genericParameters["read_during_write_mode_port_a"] = RDWBehavior2Str(portSetup.rdw);

	HCL_ASSERT(portSetup.inputRegs);

	if (portSetup.outputRegs)
		m_genericParameters["outdata_reg_a"] = "CLOCK0";
	else
		m_genericParameters["outdata_reg_a"] = "UNREGISTERED";

	if (portSetup.resetAddr)
		m_genericParameters["address_aclr_a"] = "CLEAR0";
	else
		m_genericParameters["address_aclr_a"] = "NONE";

	if (portSetup.resetWrEn)
		m_genericParameters["wrcontrol_aclr_a"] = "CLEAR0";
	else
		m_genericParameters["wrcontrol_aclr_a"] = "NONE";

	if (portSetup.outReset)
		m_genericParameters["outdata_aclr_a"] = "CLEAR0";
	else
		m_genericParameters["outdata_aclr_a"] = "NONE";

	if (portSetup.resetAddr || portSetup.resetWrEn || portSetup.outReset)
		m_resetNames[0] = "aclr0";


	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupPortB(size_t width, PortSetup portSetup)
{
	setOutputConnectionType(OUT_Q_B, {.type = hlim::ConnectionType::BITVEC, .width=width});
	m_genericParameters["width_b"] = width;
	changeInputWidth(IN_DATA_B, width);
	changeOutputWidth(OUT_Q_B, width);
	m_genericParameters["numwords_b"] = m_size / width;
	m_genericParameters["read_during_write_mode_port_b"] = RDWBehavior2Str(portSetup.rdw);


	if ((portSetup.inputRegs || portSetup.outputRegs) && portSetup.dualClock) {
		m_clockNames[1] = "clock1";
	}

	if (portSetup.inputRegs) {
		if (portSetup.dualClock) {
			m_genericParameters["rdcontrol_reg_b"] 				= "CLOCK1";
			m_genericParameters["address_reg_b"] 				= "CLOCK1";
			m_genericParameters["indata_reg_b"] 				= "CLOCK1";
			m_genericParameters["wrcontrol_wraddress_reg_b"]	= "CLOCK1";
			m_genericParameters["byteena_reg_b"] 				= "CLOCK1";
		} else {
			m_genericParameters["rdcontrol_reg_b"] 				= "CLOCK0";
			m_genericParameters["address_reg_b"] 				= "CLOCK0";
			m_genericParameters["indata_reg_b"] 				= "CLOCK0";
			m_genericParameters["wrcontrol_wraddress_reg_b"] 	= "CLOCK0";
			m_genericParameters["byteena_reg_b"]				= "CLOCK0";
		}
	} else {
		/// @todo: I think this may not be legal for altsyncram
		m_genericParameters["rdcontrol_reg_b"] 				= "UNREGISTERED";
		m_genericParameters["address_reg_b"] 				= "UNREGISTERED";
		m_genericParameters["indata_reg_b"] 				= "UNREGISTERED";
		m_genericParameters["wrcontrol_wraddress_reg_b"] 	= "UNREGISTERED";
		m_genericParameters["byteena_reg_b"]				= "UNREGISTERED";
	}

	if (portSetup.outputRegs)
		if (portSetup.dualClock)
			m_genericParameters["outdata_reg_b"] = "CLOCK1";
		else
			m_genericParameters["outdata_reg_b"] = "CLOCK0";
	else
		m_genericParameters["outdata_reg_b"] = "UNREGISTERED";

	if (portSetup.resetAddr)
		m_genericParameters["address_aclr_b"] = "CLEAR0";
	else
		m_genericParameters["address_aclr_b"] = "NONE";

	if (portSetup.resetWrEn)
		m_genericParameters["wrcontrol_aclr_b"] = "CLEAR0";
	else
		m_genericParameters["wrcontrol_aclr_b"] = "NONE";

	if (portSetup.outReset)
		m_genericParameters["outdata_aclr_b"] = "CLEAR0";
	else
		m_genericParameters["outdata_aclr_b"] = "NONE";

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
	m_genericParameters["operation_mode"] = "SINGLE_PORT";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupSimpleDualPort()
{
	m_genericParameters["operation_mode"] = "DUAL_PORT";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupTrueDualPort()
{
	m_genericParameters["operation_mode"] = "BIDIR_DUAL_PORT";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupROM()
{
	m_genericParameters["operation_mode"] = "ROM";
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupRamType(const std::string &type)
{
	//m_genericParameters["ram_block_type"] = std::move(type);
	m_genericParameters["ram_block_type"] = type;
	return *this;
}

ALTSYNCRAM &ALTSYNCRAM::setupSimulationDeviceFamily(const std::string &devFamily)
{
	//m_genericParameters["ram_block_type"] = std::move(type);
	m_genericParameters["intended_device_family"] = devFamily;
	return *this;
}


ALTSYNCRAM &ALTSYNCRAM::setupMixedPortRdw(RDWBehavior rdw)
{
	switch (rdw) {
		case RDWBehavior::DONT_CARE:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "DONT_CARE";
		break;
		case RDWBehavior::CONSTRAINED_DONT_CARE:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "CONSTRAINED_DONT_CARE";
		break;
		case RDWBehavior::OLD_DATA:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "OLD_DATA";
		break;
		case RDWBehavior::NEW_DATA_MASKED_UNDEFINED:
			m_genericParameters["read_during_write_mode_mixed_ports"] = "NEW_DATA";
		break;
	}
	return *this;
}

void ALTSYNCRAM::setInput(size_t input, const BVec &bvec)
{
	switch (input) {
		case IN_ADDRESS_A:
			m_genericParameters["widthad_a"] = bvec.size();
			changeInputWidth(IN_ADDRESS_A, bvec.size());
		break;
		case IN_BYTEENA_A:
			m_genericParameters["width_byteena_a"] = bvec.size();
			changeInputWidth(IN_BYTEENA_A, bvec.size());
		break;
		case IN_ADDRESS_B:
			m_genericParameters["widthad_b"] = bvec.size();
			changeInputWidth(IN_ADDRESS_B, bvec.size());
		break;
		case IN_BYTEENA_B:
			m_genericParameters["width_byteena_b"] = bvec.size();
			changeInputWidth(IN_BYTEENA_B, bvec.size());
		break;
		default:
		break;
	}
	ExternalComponent::setInput(input, bvec);
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
	m_genericParameters["init_file"] = filename;
	m_genericParameters["init_file_layout"] = "PORT_A";

	writeMemoryInitializationFile(stream, m_widthPortA, m_memoryInitialization);
}


hlim::OutputClockRelation ALTSYNCRAM::getOutputClockRelation(size_t output) const
{
	std::string outdata_reg;
	if (output == OUT_Q_A)
		outdata_reg = m_genericParameters.find("outdata_reg_a")->second.string();
	else
		outdata_reg = m_genericParameters.find("outdata_reg_b")->second.string();

	if (outdata_reg == "CLOCK0") 
		return { .dependentClocks={ m_clocks[0] } };

	if (outdata_reg == "CLOCK1") 
		return { .dependentClocks={ m_clocks[1] } };


	std::string addr_reg;
	if (output == OUT_Q_A)
		addr_reg =  "CLOCK0";
	else
		addr_reg = m_genericParameters.find("address_reg_b")->second.string();

	if (addr_reg == "CLOCK0") 
		return { .dependentClocks={ m_clocks[0] } };

	if (addr_reg == "CLOCK1") 
		return { .dependentClocks={ m_clocks[1] } };

	HCL_ASSERT_HINT(false, "Inconsistent configuration of ALTSYNCRAM!");
}

bool ALTSYNCRAM::checkValidInputClocks(std::span<hlim::SignalClockDomain> inputClocks) const
{
	auto clocksCompatible = [&](const hlim::Clock *clkA, const hlim::Clock *clkB) {
		if (clkA == nullptr || clkB == nullptr) return false;
		return clkA->getClockPinSource() == clkB->getClockPinSource();
	};

	auto check = [&](size_t input, const std::string &clk) {
		if (getNonSignalDriver(input).node == nullptr) return true;

		switch (inputClocks[input].type) {
			case hlim::SignalClockDomain::UNKNOWN: return false;
			case hlim::SignalClockDomain::CONSTANT: return true;
			case hlim::SignalClockDomain::CLOCK: {
				if (clk == "CLOCK0") return clocksCompatible(inputClocks[input].clk, m_clocks[0]);
				if (clk == "CLOCK1") return clocksCompatible(inputClocks[input].clk, m_clocks[1]);
				HCL_ASSERT_HINT(false, "Invalid configuration of ALTSYNCRAM!");
			}
		}
		return false;
	};

	if (!check(IN_WREN_A, "CLOCK0")) return false;
	if (!check(IN_RDEN_A, "CLOCK0")) return false;

	if (!check(IN_WREN_B, m_genericParameters.find("wrcontrol_wraddress_reg_b")->second.string())) return false;
	if (!check(IN_RDEN_B, m_genericParameters.find("rdcontrol_reg_b")->second.string())) return false;


	if (!check(IN_DATA_A, "CLOCK0")) return false;
	if (!check(IN_ADDRESS_A, "CLOCK0")) return false;
	if (!check(IN_BYTEENA_A, "CLOCK0")) return false;


	if (!check(IN_DATA_B, m_genericParameters.find("indata_reg_b")->second.string())) return false;
	if (!check(IN_ADDRESS_B, m_genericParameters.find("address_reg_b")->second.string())) return false;
	if (!check(IN_ADDRESS_B, m_genericParameters.find("wrcontrol_wraddress_reg_b")->second.string())) return false;
	if (!check(IN_BYTEENA_B, m_genericParameters.find("byteena_reg_b")->second.string())) return false;

	return true;
}

std::unique_ptr<hlim::BaseNode> ALTSYNCRAM::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new ALTSYNCRAM(m_size));
	copyBaseToClone(res.get());
	return res;
}

void ALTSYNCRAM::copyBaseToClone(BaseNode *copy) const
{
	ExternalComponent::copyBaseToClone(copy);
	auto *other = (ALTSYNCRAM*)copy;

	other->m_size = m_size;
	other->m_widthPortA = m_widthPortA;
	other->m_memoryInitialization = m_memoryInitialization;
}



}
