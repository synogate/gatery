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
#include "Node.h"

#include "NodeGroup.h"
#include "Clock.h"

#include "../utils/Exceptions.h"
#include "../utils/Range.h"

#include <algorithm>

namespace gtry::hlim {

BaseNode::BaseNode()
{
}

BaseNode::BaseNode(size_t numInputs, size_t numOutputs)
{
    resizeInputs(numInputs);
    resizeOutputs(numOutputs);
}

BaseNode::~BaseNode()
{
    HCL_ASSERT(m_refCounter == 0);
    moveToGroup(nullptr);
    for (auto i : utils::Range(m_clocks.size()))
        detachClock(i);
}


bool BaseNode::isOrphaned() const
{
    for (auto i : utils::Range(getNumInputPorts()))
        if (getDriver(i).node != nullptr) return false;

    for (auto i : utils::Range(getNumOutputPorts()))
        if (!getDirectlyDriven(i).empty()) return false;

    return true;
}


bool BaseNode::hasSideEffects() const
{
    if (hasRef()) return true;
    for (auto i : utils::Range(getNumOutputPorts()))
        if (getOutputType(i) == OUTPUT_LATCHED)
            return true;
    return false;
}

bool BaseNode::isCombinatorial() const
{
    for (auto clk : m_clocks)
        if (clk != nullptr)
            return false;
    return true;
}


void BaseNode::moveToGroup(NodeGroup *group)
{
    if (group == m_nodeGroup) return;

    if (m_nodeGroup != nullptr) {
        auto it = std::find(m_nodeGroup->m_nodes.begin(), m_nodeGroup->m_nodes.end(), this);
        HCL_ASSERT(it != m_nodeGroup->m_nodes.end());

        *it = m_nodeGroup->m_nodes.back();
        m_nodeGroup->m_nodes.pop_back();
    }

    m_nodeGroup = group;
    if (m_nodeGroup != nullptr)
        m_nodeGroup->m_nodes.push_back(this);
}

void BaseNode::attachClock(Clock *clk, size_t clockPort)
{
    if (m_clocks[clockPort] == clk) return;

    detachClock(clockPort);

    m_clocks[clockPort] = clk;
    clk->m_clockedNodes.push_back({.node = this, .port = clockPort});
}

void BaseNode::detachClock(size_t clockPort)
{
    if (m_clocks[clockPort] == nullptr) return;

    auto clock = m_clocks[clockPort];

    auto it = std::find(clock->m_clockedNodes.begin(), clock->m_clockedNodes.end(), NodePort{.node = this, .port = clockPort});
    HCL_ASSERT(it != clock->m_clockedNodes.end());

    *it = clock->m_clockedNodes.back();
    clock->m_clockedNodes.pop_back();

    m_clocks[clockPort] = nullptr;
}

void BaseNode::copyBaseToClone(BaseNode *copy) const
{
    copy->m_name = m_name;
    copy->m_comment = m_comment;
    copy->m_stackTrace = m_stackTrace;
    copy->m_clocks.resize(m_clocks.size());
    copy->resizeInputs(getNumInputPorts());
    copy->resizeOutputs(getNumOutputPorts());
    for (auto i : utils::Range(getNumOutputPorts())) {
        copy->setOutputConnectionType(i, getOutputConnectionType(i));
        copy->setOutputType(i, getOutputType(i));
    }
}

std::string BaseNode::attemptInferOutputName(size_t outputPort) const
{
    std::stringstream name;
    bool first = true;
    for (auto i : utils::Range(getNumInputPorts())) {
        auto driver = getDriver(i);
        if (driver.node == nullptr)
            return "";
        if (hlim::outputIsDependency(driver)) continue;
        if (driver.node->getName().empty()) {
            return "";
        } else {
            if (!first) name << '_';
            first = false;
            name << driver.node->getName();
        }
    }
    return name.str();
}


}
