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
#include "SignalMiscOp.h"

#include "SignalLogicOp.h"

#include <gatery/utils/Range.h>
#include <gatery/utils/Preprocessor.h>


namespace gtry {


SignalTapHelper::SignalTapHelper(hlim::Node_SignalTap::Level level)
{
    m_node = DesignScope::createNode<hlim::Node_SignalTap>();
    m_node->recordStackTrace();
    m_node->setLevel(level);
}

size_t SignalTapHelper::addInput(hlim::NodePort nodePort)
{
    for (auto i : utils::Range(m_node->getNumInputPorts()))
        if (m_node->getDriver(i) == nodePort)
            return i;
    m_node->addInput(nodePort);
    return m_node->getNumInputPorts()-1;
}

void SignalTapHelper::triggerIf(const Bit &condition)
{
    HCL_ASSERT_HINT(m_node->getNumInputPorts() == 0, "Condition must be the first input to signal tap!");
    addInput(condition.getReadPort());
    m_node->setTrigger(hlim::Node_SignalTap::TRIG_FIRST_INPUT_HIGH);
}

void SignalTapHelper::triggerIfNot(const Bit &condition)
{
    HCL_ASSERT_HINT(m_node->getNumInputPorts() == 0, "Condition must be the first input to signal tap!");
    addInput(condition.getReadPort());
    m_node->setTrigger(hlim::Node_SignalTap::TRIG_FIRST_INPUT_LOW);
}


SignalTapHelper &SignalTapHelper::operator<<(const std::string &msg)
{
    m_node->addMessagePart(msg);
    return *this;
}

BVec swapEndian(const BVec& word, size_t byteSize)
{
    const size_t numSymbols = (word.size() + byteSize - 1) / byteSize;
    const size_t srcWidth = numSymbols * byteSize;

    hlim::Node_Rewire* rewire = DesignScope::createNode<hlim::Node_Rewire>(1);
    rewire->connectInput(0, word.getReadPort().expand(srcWidth, hlim::ConnectionType::BITVEC));

    hlim::Node_Rewire::RewireOperation op;
    op.ranges.reserve(numSymbols);
    for (size_t i = 0; i < numSymbols; ++i)
        op.addInput(0, srcWidth - byteSize * (i + 1), byteSize);
    rewire->setOp(std::move(op));

    BVec ret{ SignalReadPort(rewire) };
    if (!word.getName().empty())
        ret.setName(std::string(word.getName()) + "_swapped");
    return ret;
}

SignalTapHelper sim_assert(const Bit &condition)
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_ASSERT);
    helper.triggerIfNot(/*(!ConditionalScope::getCurrentCondition()) | */condition);
    return helper;
}

SignalTapHelper sim_warnIf(const Bit &condition)
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_WARN);
    helper.triggerIf(/*ConditionalScope::getCurrentCondition() & */condition);
    return helper;
}


SignalTapHelper sim_debug()
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
    if (ConditionalScope::get() != nullptr)
        helper.triggerIf(Bit(SignalReadPort{ConditionalScope::get()->getFullCondition()}));
    return helper;
}

SignalTapHelper sim_debugAlways()
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
    return helper;
}

SignalTapHelper sim_debugIf(const Bit &condition)
{
    SignalTapHelper helper(hlim::Node_SignalTap::LVL_DEBUG);
    helper.triggerIf(/*ConditionalScope::getCurrentCondition() & */condition);
    return helper;
}





}
