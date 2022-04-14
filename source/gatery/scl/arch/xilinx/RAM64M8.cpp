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

#include "RAM64M8.h"

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::xilinx {

RAM64M8::RAM64M8()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "RAM64M8";
	m_isEntity = false;
	m_clockNames = {"WCLK"};
	m_resetNames = {""};
	m_clocks.resize(CLK_COUNT);

	
	resizeInputs(IN_COUNT);
	resizeOutputs(OUT_COUNT);

	setOutputConnectionType(OUT_DO_A, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_B, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_C, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_D, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_E, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_F, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_G, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
	setOutputConnectionType(OUT_DO_H, {.interpretation = hlim::ConnectionType::BOOL, .width=1});
}

UInt RAM64M8::setup64x7_SDP(const UInt &wrAddr, const UInt &wrData, const Bit &wrEn, const UInt &rdAddr)
{
	Bit zero = '0';
	NodeIO::connectInput(IN_DI_H, zero.readPort());

	HCL_ASSERT(wrAddr.size() == 6);
	NodeIO::connectInput(IN_ADDR_H, wrAddr.readPort());

	HCL_ASSERT(wrData.size() == 7);
	for (auto i : utils::Range(wrData.size()))
		NodeIO::connectInput(IN_DI_A+i, (wrData[i]).readPort());

	NodeIO::connectInput(IN_WE, wrEn.readPort());

	HCL_ASSERT(rdAddr.size() == 6);
	for (auto i : utils::Range(7))
		NodeIO::connectInput(IN_ADDR_A+i, rdAddr.readPort());

	std::array<Bit, 7> readBits;
	for (auto i : utils::Range(7))
		readBits[i] = SignalReadPort(hlim::NodePort{this, static_cast<size_t>(OUT_DO_A+i)});

	return pack(readBits);
}


std::string RAM64M8::getTypeName() const
{
	return m_name;
}

void RAM64M8::assertValidity() const
{
}

std::string RAM64M8::getInputName(size_t idx) const
{
	switch (idx) {
		case IN_DI_A: return "DIA";
		case IN_DI_B: return "DIB";
		case IN_DI_C: return "DIC";
		case IN_DI_D: return "DID";
		case IN_DI_E: return "DIE";
		case IN_DI_F: return "DIF";
		case IN_DI_G: return "DIG";
		case IN_DI_H: return "DIH";
		case IN_ADDR_A: return "ADDRA";
		case IN_ADDR_B: return "ADDRB";
		case IN_ADDR_C: return "ADDRC";
		case IN_ADDR_D: return "ADDRD";
		case IN_ADDR_E: return "ADDRE";
		case IN_ADDR_F: return "ADDRF";
		case IN_ADDR_G: return "ADDRG";
		case IN_ADDR_H: return "ADDRH";
		case IN_WE: return "WE";
		default: return "";
	}
}

std::string RAM64M8::getOutputName(size_t idx) const
{
	switch (idx) {
		case OUT_DO_A: return "DOA";
		case OUT_DO_B: return "DOB";
		case OUT_DO_C: return "DOC";
		case OUT_DO_D: return "DOD";
		case OUT_DO_E: return "DOE";
		case OUT_DO_F: return "DOF";
		case OUT_DO_G: return "DOG";
		case OUT_DO_H: return "DOH";
		default: return "";
	}
}

std::unique_ptr<hlim::BaseNode> RAM64M8::cloneUnconnected() const
{
	RAM64M8 *ptr;
	std::unique_ptr<BaseNode> res(ptr = new RAM64M8());
	copyBaseToClone(res.get());

	return res;
}

std::string RAM64M8::attemptInferOutputName(size_t outputPort) const
{
	return m_name + '_' + getOutputName(outputPort);
}




}
