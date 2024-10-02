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

#include "RAM64M8.h"

#include <gatery/frontend/UInt.h>
#include <gatery/frontend/SInt.h>
#include <gatery/frontend/Pack.h>

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

	
	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_DI_A, "DIA");
	declInputBit(IN_DI_B, "DIB");
	declInputBit(IN_DI_C, "DIC");
	declInputBit(IN_DI_D, "DID");
	declInputBit(IN_DI_E, "DIE");
	declInputBit(IN_DI_F, "DIF");
	declInputBit(IN_DI_G, "DIG");
	declInputBit(IN_DI_H, "DIH");

	declInputBitVector(IN_ADDR_A, "ADDRA", 6);
	declInputBitVector(IN_ADDR_B, "ADDRB", 6);
	declInputBitVector(IN_ADDR_C, "ADDRC", 6);
	declInputBitVector(IN_ADDR_D, "ADDRD", 6);
	declInputBitVector(IN_ADDR_E, "ADDRE", 6);
	declInputBitVector(IN_ADDR_F, "ADDRF", 6);
	declInputBitVector(IN_ADDR_G, "ADDRG", 6);
	declInputBitVector(IN_ADDR_H, "ADDRH", 6);

	declInputBit(IN_WE, "WE");

	declOutputBit(OUT_DO_A, "DOA");
	declOutputBit(OUT_DO_B, "DOB");
	declOutputBit(OUT_DO_C, "DOC");
	declOutputBit(OUT_DO_D, "DOD");
	declOutputBit(OUT_DO_E, "DOE");
	declOutputBit(OUT_DO_F, "DOF");
	declOutputBit(OUT_DO_G, "DOG");
	declOutputBit(OUT_DO_H,	"DOH");
}

void RAM64M8::setInitialization(sim::DefaultBitVectorState memoryInitialization)
{
	m_memoryInitialization = std::move(memoryInitialization);
	if (sim::anyDefined(m_memoryInitialization)) {
		HCL_ASSERT(m_memoryInitialization.size() <= 64*8);
		for (auto i : utils::Range((m_memoryInitialization.size()+63)/64)) {
			auto start = i * 64;
			auto remaining = std::min<size_t>(m_memoryInitialization.size() - i, 64);
			std::uint64_t word = m_memoryInitialization.extractNonStraddling(sim::DefaultConfig::VALUE, start, remaining);
			std::uint64_t defined = m_memoryInitialization.extractNonStraddling(sim::DefaultConfig::DEFINED, start, remaining);
			word &= defined;

			std::string key = "INIT_";
			key += 'A' + (char)i;
			m_genericParameters[key].setBitVector(64, word, GenericParameter::BitFlavor::BIT);
		}
	}
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

void RAM64M8::copyBaseToClone(BaseNode *copy) const
{
	ExternalComponent::copyBaseToClone(copy);
	auto *other = (RAM64M8*)copy;

	other->m_memoryInitialization = m_memoryInitialization;
}


}
