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
#include "BitVectorSlice.h"

#include "UInt.h"
#include "DesignScope.h"
#include <gatery/hlim/coreNodes/Node_Multiplexer.h>
#include <gatery/hlim/coreNodes/Node_Rewire.h>

using namespace gtry;

static hlim::Node_Rewire::RewireOperation replaceSelection(size_t rangeOffset, size_t rangeWidth, size_t totalWidth)
{
	HCL_ASSERT(rangeOffset < totalWidth);

	hlim::Node_Rewire::RewireOperation op;

	op.addInput(0, 0, rangeOffset);
	op.addInput(1, 0, std::min(rangeWidth, totalWidth - rangeOffset));
	if(totalWidth > rangeOffset + rangeWidth)
		op.addInput(0, rangeOffset + rangeWidth, totalWidth - (rangeOffset + rangeWidth));

	return op;
}

SignalReadPort gtry::BitVectorSlice::assign(SignalReadPort current, SignalReadPort next) const
{
	std::function<SignalReadPort(SignalReadPort)> fun = [next](SignalReadPort current) {
		return next;
	};

	for(const BitVectorSlice* it = this; it; it = it->m_parent.get())
		fun = std::bind(&BitVectorSlice::assignLocal, it, std::placeholders::_1, fun);

	return fun(current);
}

gtry::BitVectorSliceStatic::BitVectorSliceStatic(const Selection& s, BitWidth parentW, const std::shared_ptr<BitVectorSlice>& r)
{
	m_parent = r;

	if (s.start >= 0)
		m_offset = size_t(s.start);
	else
		m_offset = size_t(s.start + parentW.bits());

	if (s.untilEndOfSource)
		m_width = parentW.bits() - m_offset;
	else if (s.width >= 0)
		m_width = size_t(s.width);
	else
		m_width = size_t(s.width + parentW.bits());
}

bool gtry::BitVectorSliceStatic::operator < (const BitVectorSliceStatic& other) const
{
	if (m_parent.get() != other.m_parent.get())
		return m_parent.get() < other.m_parent.get();
	if (m_offset != other.m_offset)
		return m_offset < other.m_offset;
	return m_width < other.m_width;
}

SignalReadPort gtry::BitVectorSliceStatic::readPort(SignalReadPort rootPort) const
{
	hlim::Node_Rewire::RewireOperation op;
	op.addInput(0, m_offset, m_width);

	auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
	SignalReadPort basePort = m_parent ? m_parent->readPort(rootPort) : rootPort;
	rewire->connectInput(0, basePort);
	rewire->setOp(op);

	HCL_DESIGNCHECK_HINT(m_offset + m_width <= hlim::getOutputWidth(basePort), "Slice offset+width is larger than source width!");

	if (m_isBit)
		rewire->changeOutputType({ .type = hlim::ConnectionType::Type::BOOL, .width = 1 });

	return SignalReadPort(rewire);
}

SignalReadPort gtry::BitVectorSliceStatic::assignLocal(SignalReadPort current, std::function<SignalReadPort(SignalReadPort)> childAssign) const
{
	auto* extract = DesignScope::createNode<hlim::Node_Rewire>(1);
	extract->connectInput(0, current);
	extract->setExtract(m_offset, m_width);

	auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
	rewire->connectInput(0, current);
	rewire->connectInput(1, childAssign(SignalReadPort(extract)));
	
	size_t currentW = current.node->getOutputConnectionType(current.port).width;
	rewire->setOp(replaceSelection(m_offset, m_width, currentW));
	return SignalReadPort(rewire);
}

gtry::BitVectorSliceDynamic::BitVectorSliceDynamic(const UInt& dynamicOffset, size_t maxOffset, size_t dynamicOffsetMul, BitWidth w, const std::shared_ptr<BitVectorSlice>& r)
{
	m_parent = r;
	m_width = w.bits();

	m_maxDynamicIndex = maxOffset; // parent->width / dynamicOffsetMul - 1;
	m_offsetDynamicMul = dynamicOffsetMul;

	SignalReadPort idxPort = dynamicOffset.readPort();
	m_offsetDynamic.node = idxPort.node;
	m_offsetDynamic.port = idxPort.port;
}

bool gtry::BitVectorSliceDynamic::operator<(const BitVectorSliceDynamic& other) const
{
	if (m_parent.get() != other.m_parent.get())
		return m_parent.get() < other.m_parent.get();
	if (m_offsetDynamicMul != other.m_offsetDynamicMul)
		return m_offsetDynamicMul < other.m_offsetDynamicMul;
	if (m_maxDynamicIndex != other.m_maxDynamicIndex)
		return m_maxDynamicIndex < other.m_maxDynamicIndex;
	if (m_offsetDynamic != other.m_offsetDynamic)
		return m_offsetDynamic < other.m_offsetDynamic;
	return m_width < other.m_width;
}

SignalReadPort BitVectorSliceDynamic::readPort(SignalReadPort rootPort) const
{
	SignalReadPort basePort = m_parent ? m_parent->readPort(rootPort) : rootPort;
	size_t fullOptions = m_maxDynamicIndex + 1;

	/// @todo do we allow dynamically slicing words that are partially out of bounds?
	auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(fullOptions);
	mux->connectSelector(m_offsetDynamic);

	for (auto i : utils::Range(fullOptions)) {
		auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
		rewire->connectInput(0, basePort);
		rewire->setExtract(i * m_offsetDynamicMul, m_width);

		if (m_isBit)
			rewire->changeOutputType({ .type = hlim::ConnectionType::Type::BOOL, .width = 1 });

		mux->connectInput(i, { .node = rewire, .port = 0ull });
	}
	return SignalReadPort(mux);
}

SignalReadPort gtry::BitVectorSliceDynamic::assignLocal(SignalReadPort current, std::function<SignalReadPort(SignalReadPort)> childAssign) const
{
	auto* mux = DesignScope::createNode<hlim::Node_Multiplexer>(m_maxDynamicIndex + 1);
	mux->connectSelector(m_offsetDynamic);

	for (auto i : utils::Range(m_maxDynamicIndex + 1)) {
		auto* extract = DesignScope::createNode<hlim::Node_Rewire>(1);
		extract->connectInput(0, current);
		extract->setExtract(i * m_offsetDynamicMul, m_width);

		auto* rewire = DesignScope::createNode<hlim::Node_Rewire>(2);
		rewire->connectInput(0, current);
		rewire->connectInput(1, childAssign(SignalReadPort(extract)));

		size_t currentW = current.node->getOutputConnectionType(current.port).width;
		rewire->setOp(replaceSelection(i * m_offsetDynamicMul, m_width, currentW));

		mux->connectInput(i, { .node = rewire, .port = 0ull });
	}
	return SignalReadPort(mux);
}
