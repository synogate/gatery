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
#include "BUFG.h"

#include <gatery/hlim/NodeGroup.h>

#include <gatery/frontend/GraphTools.h>
#include <gatery/frontend/DesignScope.h>
#include <gatery/debug/DebugInterface.h>
#include <gatery/utils/Exceptions.h>
#include <gatery/utils/Preprocessor.h>

namespace gtry::scl::arch::xilinx {

BUFG::BUFG()
{
	m_libraryName = "UNISIM";
	m_packageName = "VCOMPONENTS";
	m_name = "BUFG";
	m_isEntity = false;
	m_clockNames = {};
	m_resetNames = {};

	resizeIOPorts(1, 1);
	declInputBit(0, "I", BitFlavor::STD_ULOGIC);
	declOutputBit(0, "O", BitFlavor::STD_ULOGIC);
}

std::unique_ptr<hlim::BaseNode> BUFG::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new BUFG());
	copyBaseToClone(res.get());

	return res;
}


std::string BUFG::attemptInferOutputName(size_t outputPort) const
{
	auto driver = getDriver(0);
	
	if (driver.node == nullptr)
		return ExternalComponent::attemptInferOutputName(outputPort);

	if (inputIsComingThroughParentNodeGroup(0))
		return ExternalComponent::attemptInferOutputName(outputPort);
	
	if (driver.node->getName().empty())
		return ExternalComponent::attemptInferOutputName(outputPort);

	return driver.node->getName() + "_global";
}


bool BUFGPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	if (nodeGroup->getName() != "scl_globalBuffer") return false;


	NodeGroupIO io(nodeGroup);

	if (io.inputBits.contains("globalBufferPlaceholder")) {
		Bit &input = io.inputBits["globalBufferPlaceholder"];

		HCL_ASSERT_HINT(io.outputBits.contains("globalBufferPlaceholder"), "Missing output for global buffer, probably because not yet implemented for bundles!");
		Bit &output = io.outputBits["globalBufferPlaceholder"];

		auto *bufg = DesignScope::createNode<BUFG>();
		bufg->setInput(0, input);
		output.exportOverride(bufg->getOutputBit(0));
	} else
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with BUFG because the 'globalBufferPlaceholder' signal could not be found or is not a bit!");


	return true;
}



}
