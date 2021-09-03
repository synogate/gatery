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
#include "BUFG.h"

#include <gatery/frontend/GraphTools.h>
#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::arch::xilinx {

BUFG::BUFG()
{
    m_libraryName = "UNIMACRO";
    m_packageName = "VCOMPONENTS";
    m_name = "BUFG";
    m_isEntity = false;
    m_clockNames = {};
    m_resetNames = {};

    resizeInputs(1);
    resizeOutputs(1);
}

void BUFG::connectInput(const Bit &bit)
{
	connectInput(bit.getReadPort());
}
/*
void BUFG::connectInput(const BVec &bvec)
{
	connectInput(bvec.getReadPort());
}
*/

void BUFG::connectInput(hlim::NodePort nodePort)
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

std::string BUFG::getTypeName() const
{
    return "BUFG";
}

void BUFG::assertValidity() const
{
}

std::string BUFG::getInputName(size_t idx) const
{
    return "I";
}

std::string BUFG::getOutputName(size_t idx) const
{
    return "O";
}

std::unique_ptr<hlim::BaseNode> BUFG::cloneUnconnected() const
{
    BUFG *ptr;
    std::unique_ptr<BaseNode> res(ptr = new BUFG());
    copyBaseToClone(res.get());

    ptr->m_libraryName = m_libraryName;
    ptr->m_name = m_name;
    ptr->m_genericParameters = m_genericParameters;

    return res;
}

std::string BUFG::attemptInferOutputName(size_t outputPort) const
{
    auto driver = getDriver(0);
    
	if (driver.node == nullptr)
		return "";
    
	if (driver.node->getName().empty())
		return "";

	return driver.node->getName() + "_global";
}


bool BUFGPattern::attemptApply(hlim::NodeGroup *nodeGroup)
{
	if (nodeGroup->getName() != "scl_globalBuffer") return false;


	/// @todo: Make it work for bundles as well

//	HCL_ASSERT_HINT(nodeGroup->getNodes().size() == 1, "scl_globalBuffer should only contain a single signal node (not working for bundles yet)");
//	HCL_ASSERT_HINT(dynamic_cast<hlim::Node_Signal*>(nodeGroup->getNodes().front()), "scl_globalBuffer should only contain a single signal node (not working for bundles yet)");

	NodeGroupIO io(nodeGroup);

	if (io.inputBits.contains("globalBufferPlaceholder")) {
		Bit &input = io.inputBits["globalBufferPlaceholder"];

		HCL_ASSERT_HINT(io.outputBits.contains("globalBufferPlaceholder"), "Missing output for global buffer, probably because not yet implemented for bundles!");
		Bit &output = io.outputBits["globalBufferPlaceholder"];

    	auto *bufg = DesignScope::createNode<BUFG>();
		bufg->connectInput(input);
	    output.setExportOverride(SignalReadPort(bufg));
	}/* else {
		
		HCL_ASSERT_HINT(io.inputBVecs.contains("globalBufferPlaceholder"), "Missing output for global buffer, probably because not yet implemented for bundles!");
		BVec &input = io.inputBVecs["globalBufferPlaceholder"];

		HCL_ASSERT_HINT(io.outputBVecs.contains("globalBufferPlaceholder"), "Missing output for global buffer, probably because not yet implemented for bundles!");
		BVec &output = io.outputBVecs["globalBufferPlaceholder"];

		HCL_ASSERT(input.getWidth() == output.getWidth());

    	auto *bufg = DesignScope::createNode<BUFG>();
		bufg->connectInput(input);
	    output.setExportOverride(SignalReadPort(bufg));
	}*/

	return true;
}



}
