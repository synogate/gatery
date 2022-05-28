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
#include "TRI.h"

#include <gatery/frontend/GraphTools.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::arch::intel {

TRI::TRI()
{
	m_libraryName = "altera";
	m_packageName = "altera_primitives_components";
	m_name = "TRI";
	m_clockNames = {};
	m_resetNames = {};

	resizeInputs(2);
	resizeOutputs(1);
}

void TRI::connectInput(const Bit &bit)
{
	connectInput(bit.readPort());
}

void TRI::connectInput(const UInt &bvec)
{
	connectInput(bvec.readPort());
}

void TRI::connectEnable(const Bit &bit)
{
	NodeIO::connectInput(1, bit.readPort());
}


void TRI::connectInput(hlim::NodePort nodePort)
{
	if (nodePort.node != nullptr) {
		auto paramType = nodePort.node->getOutputConnectionType(nodePort.port);
		auto myType = getOutputConnectionType(0);
		if (!getDirectlyDriven(0).empty()) {
			HCL_ASSERT_HINT( paramType == myType, "The connection type of a node that is driving other nodes can not change");
		} else
			setOutputConnectionType(0, paramType);
	}
	NodeIO::connectInput(0, nodePort);
}

std::string TRI::getTypeName() const
{
	return "TRI";
}

void TRI::assertValidity() const
{
}

std::string TRI::getInputName(size_t idx) const
{
	if (idx == 0)
		return "a_in";
	else
		return "oe";
}

std::string TRI::getOutputName(size_t idx) const
{
	return "a_out";
}

std::unique_ptr<hlim::BaseNode> TRI::cloneUnconnected() const
{
	TRI *ptr;
	std::unique_ptr<BaseNode> res(ptr = new TRI());
	copyBaseToClone(res.get());

	return res;
}

std::string TRI::attemptInferOutputName(size_t outputPort) const
{
	auto driver = getDriver(0);
	
	if (driver.node == nullptr)
		return "";
	
	if (driver.node->getName().empty())
		return "";

	return driver.node->getName() + "_tristate";
}


bool TRIPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	if (nodeGroup->getName() != "scl_tristate_output") return false;

	NodeGroupIO io(nodeGroup);

	HCL_ASSERT_HINT(io.inputBits.contains("outputEnable"), "Missing outputEnable for Tristate Output!");
	Bit &outputEnable = io.inputBits["outputEnable"];

	if (io.inputBits.contains("signal")) {
		Bit &input = io.inputBits["signal"];

		HCL_ASSERT_HINT(io.outputBits.contains("result"), "Missing output for Tristate Output!");
		Bit &output = io.outputBits["result"];

		auto *tri = DesignScope::createNode<TRI>();
		tri->connectInput(input);
		tri->connectEnable(outputEnable);
		output.exportOverride(SignalReadPort(tri));
	} else
	if (io.inputUInts.contains("signal")) {
		UInt &input = io.inputUInts["signal"];

		HCL_ASSERT_HINT(io.outputUInts.contains("result"), "Missing output for Tristate Output!");
		UInt &output = io.outputUInts["result"];

		auto *tri = DesignScope::createNode<TRI>();
		tri->connectInput(input);
		tri->connectEnable(outputEnable);
		output.exportOverride(SignalReadPort(tri));
	} else
		HCL_ASSERT_HINT(false, "Missing signal for Tristate Output!");

	return true;
}



}
