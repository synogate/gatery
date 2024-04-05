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
#include "TRI.h"

#include <gatery/hlim/NodeGroup.h>

#include <gatery/frontend/DesignScope.h>
#include <gatery/frontend/GraphTools.h>
#include <gatery/frontend/Constant.h>

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

	resizeIOPorts(IN_COUNT, OUT_COUNT);
	declInputBit(IN_A_IN, "A_IN");
	declInputBit(IN_OE, "OE");
	declOutputBit(OUT_A_OUT, "A_OUT");
}

std::unique_ptr<hlim::BaseNode> TRI::cloneUnconnected() const
{
	std::unique_ptr<BaseNode> res(new TRI());
	copyBaseToClone(res.get());
	return res;
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
		tri->setInput(TRI::IN_A_IN, input);
		tri->setInput(TRI::IN_OE, outputEnable);
		output.exportOverride(SignalReadPort(tri));
	} else
	if (io.inputBVecs.contains("signal")) {
		BVec &input = io.inputBVecs["signal"];

		HCL_ASSERT_HINT(io.outputBVecs.contains("result"), "Missing output for Tristate Output!");
		BVec &output = io.outputBVecs["result"];

		HCL_ASSERT(input.width() == output.width());
		BVec overrideBits = ConstBVec(0, input.width());
		for (auto i : utils::Range(input.size())) {
			auto *tri = DesignScope::createNode<TRI>();
			tri->setInput(TRI::IN_A_IN, input[i]);
			tri->setInput(TRI::IN_OE, outputEnable);
			Bit outBit = SignalReadPort(tri);
			overrideBits[i] = outBit;
		}
		output.exportOverride(overrideBits);
	} else
		HCL_ASSERT_HINT(false, "Missing signal for Tristate Output!");

	return true;
}



}
