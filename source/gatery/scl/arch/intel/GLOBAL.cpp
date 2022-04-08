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
#include "GLOBAL.h"

#include <gatery/frontend/GraphTools.h>

#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::arch::intel {

GLOBAL::GLOBAL()
{
    m_libraryName = "altera";
    m_packageName = "altera_primitives_components";
    m_name = "GLOBAL";
    m_clockNames = {};
    m_resetNames = {};

    resizeInputs(1);
    resizeOutputs(1);
}

void GLOBAL::connectInput(const Bit &bit)
{
	connectInput(bit.readPort());
}

/*
void GLOBAL::connectInput(const BVec &bvec)
{
	connectInput(bvec.readPort());
}
*/

void GLOBAL::connectInput(hlim::NodePort nodePort)
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

std::string GLOBAL::getTypeName() const
{
    return "GLOBAL";
}

void GLOBAL::assertValidity() const
{
}

std::string GLOBAL::getInputName(size_t idx) const
{
    return "a_in";
}

std::string GLOBAL::getOutputName(size_t idx) const
{
    return "a_out";
}

std::unique_ptr<hlim::BaseNode> GLOBAL::cloneUnconnected() const
{
    GLOBAL *ptr;
    std::unique_ptr<BaseNode> res(ptr = new GLOBAL());
    copyBaseToClone(res.get());

    return res;
}

std::string GLOBAL::attemptInferOutputName(size_t outputPort) const
{
    auto driver = getDriver(0);
    
	if (driver.node == nullptr)
		return "";
    
	if (driver.node->getName().empty())
		return "";

	return driver.node->getName() + "_global";
}


bool GLOBALPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	if (nodeGroup->getName() != "scl_globalBuffer") return false;

//	HCL_ASSERT_HINT(nodeGroup->getNodes().size() == 1, "scl_globalBuffer should only contain a single signal node (not working for bundles yet)");
//	HCL_ASSERT_HINT(dynamic_cast<hlim::Node_Signal*>(nodeGroup->getNodes().front()), "scl_globalBuffer should only contain a single signal node (not working for bundles yet)");

	NodeGroupIO io(nodeGroup);

	if (io.inputBits.contains("globalBufferPlaceholder")) {
		Bit &input = io.inputBits["globalBufferPlaceholder"];

		HCL_ASSERT_HINT(io.outputBits.contains("globalBufferPlaceholder"), "Missing output for global buffer!");
		Bit &output = io.outputBits["globalBufferPlaceholder"];

    	auto *global = DesignScope::createNode<GLOBAL>();
		global->connectInput(input);
	    output.exportOverride(SignalReadPort(global));
	}

	return true;
}



}
