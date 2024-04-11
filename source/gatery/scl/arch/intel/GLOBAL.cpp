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
#include "GLOBAL.h"

#include <gatery/hlim/NodeGroup.h>

#include <gatery/debug/DebugInterface.h>
#include <gatery/frontend/GraphTools.h>
#include <gatery/frontend/DesignScope.h>

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

	resizeIOPorts(1, 1);

	declInputBit(0, "A_IN");
	declOutputBit(0, "A_OUT");
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
		return ExternalComponent::attemptInferOutputName(outputPort);

	if (inputIsComingThroughParentNodeGroup(0))
		return ExternalComponent::attemptInferOutputName(outputPort);

	if (driver.node->getName().empty())
		return ExternalComponent::attemptInferOutputName(outputPort);

	return driver.node->getName() + "_global";
}


bool GLOBALPattern::scopedAttemptApply(hlim::NodeGroup *nodeGroup) const
{
	if (nodeGroup->getName() != "scl_globalBuffer") return false;

	dbg::log(dbg::LogMessage(nodeGroup) << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING << "Replacing " << nodeGroup << " with GLOBAL macro");

//	HCL_ASSERT_HINT(nodeGroup->getNodes().size() == 1, "scl_globalBuffer should only contain a single signal node (not working for bundles yet)");
//	HCL_ASSERT_HINT(dynamic_cast<hlim::Node_Signal*>(nodeGroup->getNodes().front()), "scl_globalBuffer should only contain a single signal node (not working for bundles yet)");

	NodeGroupIO io(nodeGroup);

	if (io.inputBits.contains("globalBufferPlaceholder")) {
		Bit &input = io.inputBits["globalBufferPlaceholder"];

		HCL_ASSERT_HINT(io.outputBits.contains("globalBufferPlaceholder"), "Missing output for global buffer!");
		Bit &output = io.outputBits["globalBufferPlaceholder"];

		auto *global = DesignScope::createNode<GLOBAL>();
		global->setInput(0, input);
		output.exportOverride(global->getOutputBit(0));
	} else
		dbg::log(dbg::LogMessage{nodeGroup} << dbg::LogMessage::LOG_INFO << dbg::LogMessage::LOG_TECHNOLOGY_MAPPING 
				<< "Not replacing " << nodeGroup << " with GLOBAL because the 'globalBufferPlaceholder' signal could not be found or is not a bit!");

	return true;
}



}
