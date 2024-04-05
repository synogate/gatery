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

#include "RAM256X1D.h"

#include <gatery/frontend/UInt.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

#include <boost/format.hpp>

namespace gtry::scl::arch::xilinx {

RAM256X1D::RAM256X1D()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "RAM256X1D";
	m_isEntity = false;
	m_clockNames = {"WCLK"};
	m_resetNames = {""};
	m_clocks.resize(CLK_COUNT);

	resizeIOPorts(IN_COUNT, OUT_COUNT);

	declInputBit(IN_D, "D");
	declInputBitVector(IN_A, "A", 8);
	declInputBitVector(IN_DPRA, "DPRA", 8);
	declInputBit(IN_WE, "WE");


	declOutputBit(OUT_SPO, "SPO");
	declOutputBit(OUT_DPO, "DPO");
}

void RAM256X1D::setInitialization(sim::DefaultBitVectorState memoryInitialization)
{
	m_memoryInitialization = std::move(memoryInitialization);
	if (sim::anyDefined(m_memoryInitialization)) {
		HCL_ASSERT(m_memoryInitialization.size() <= 256);
		sim::DefaultBitVectorState init256;
		init256.resize(256);
		init256.clearRange(sim::DefaultConfig::VALUE, 0, 256);
		init256.clearRange(sim::DefaultConfig::DEFINED, 0, 256);
		init256.copyRange(0, m_memoryInitialization, 0, m_memoryInitialization.size());

		m_genericParameters["INIT"].setBitVector(init256);
	}
}

Bit RAM256X1D::setupSDP(const UInt &wrAddr, const Bit &wrData, const Bit &wrEn, const UInt &rdAddr)
{
	HCL_ASSERT(wrAddr.size() == 8);
	NodeIO::connectInput(IN_A, wrAddr.readPort());

	NodeIO::connectInput(IN_D, wrData.readPort());

	NodeIO::connectInput(IN_WE, wrEn.readPort());

	HCL_ASSERT(rdAddr.size() == 8);
	NodeIO::connectInput(IN_DPRA, rdAddr.readPort());

	return SignalReadPort(hlim::NodePort{this, static_cast<size_t>(OUT_DPO)});
}


std::string RAM256X1D::getTypeName() const
{
	return m_name;
}

void RAM256X1D::assertValidity() const
{
}

std::unique_ptr<hlim::BaseNode> RAM256X1D::cloneUnconnected() const
{
	RAM256X1D *ptr;
	std::unique_ptr<BaseNode> res(ptr = new RAM256X1D());
	copyBaseToClone(res.get());

	return res;
}

std::string RAM256X1D::attemptInferOutputName(size_t outputPort) const
{
	return m_name + '_' + getOutputName(outputPort);
}


void RAM256X1D::copyBaseToClone(BaseNode *copy) const
{
	ExternalComponent::copyBaseToClone(copy);
	auto *other = (RAM256X1D*)copy;

	other->m_memoryInitialization = m_memoryInitialization;
}



}
