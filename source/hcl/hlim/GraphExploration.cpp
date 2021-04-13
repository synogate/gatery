/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "hcl/pch.h"
#include "GraphExploration.h"

#include "coreNodes/Node_Signal.h"

#include "../utils/Range.h"


namespace hcl::hlim {

template<bool forward, typename Policy>
Exploration<forward, Policy>::NodePortHandle::NodePortHandle(iterator &iterator, NodePort nodePort) : m_iterator(iterator), m_nodePort(nodePort) { }

template<bool forward, typename Policy>
bool Exploration<forward, Policy>::NodePortHandle::isSignal() const
{
    return dynamic_cast<const Node_Signal*>(node()) != nullptr;
}


template<bool forward, typename Policy>
bool Exploration<forward, Policy>::NodePortHandle::isBranchingForward()
{
    size_t numConsumers = 0;
    for (auto i : utils::Range(node()->getNumOutputPorts())) {
        numConsumers += node()->getDirectlyDriven(i).size();
        if (numConsumers > 1) return true;
    }
    return false;
}

template<bool forward, typename Policy>
bool Exploration<forward, Policy>::NodePortHandle::isBranchingBackward()
{
    size_t numDrivers = 0;
    for (auto i : utils::Range(node()->getNumInputPorts())) {
        if (node()->getDriver(i).node != nullptr) {
            numDrivers++;
            if (numDrivers > 1) return true;
        }
    }
    return false;
}

template<bool forward, typename Policy>
void Exploration<forward, Policy>::NodePortHandle::backtrack()
{
    m_iterator.backtrack();
}





template<bool forward, typename Policy>
Exploration<forward, Policy>::Exploration(NodePort nodePort) : m_nodePort(nodePort)
{
}

template<bool forward, typename Policy>
typename Exploration<forward, Policy>::iterator Exploration<forward, Policy>::begin()
{
    return iterator(m_skipDependencies, m_nodePort);
}

template<bool forward, typename Policy>
typename Exploration<forward, Policy>::iterator Exploration<forward, Policy>::end()
{
    return iterator();
}


template<bool forward>
void DepthFirstPolicy<forward>::init(NodePort nodePort)
{
    if (forward) {
        if (nodePort.node != nullptr)
            for (auto np : nodePort.node->getDirectlyDriven(nodePort.port))
                m_stack.push(np);
    } else {
        if (nodePort.node != nullptr) {
            auto driver = nodePort.node->getDriver(nodePort.port);
            if (driver.node != nullptr)
                m_stack.push(driver);
        }
    }
}

template<bool forward>
void DepthFirstPolicy<forward>::advance(bool skipDependencies)
{
    if (m_stack.empty()) return;
    BaseNode *currentNode = m_stack.top().node;
    m_stack.pop();
    if (forward) {
        for (auto i : utils::Range(currentNode->getNumOutputPorts()))
            if (!skipDependencies ||  currentNode->getOutputConnectionType(i).interpretation != ConnectionType::DEPENDENCY)
                for (auto np : currentNode->getDirectlyDriven(i))
                    m_stack.push(np);
    } else {
        for (auto i : utils::Range(currentNode->getNumInputPorts())) {
            auto driver = currentNode->getDriver(i);
            if (driver.node != nullptr)
                if (!skipDependencies || !hlim::outputIsDependency(driver))
                    m_stack.push(driver);
        }
    }
}

template<bool forward>
void DepthFirstPolicy<forward>::backtrack()
{
    m_stack.pop();
}

template<bool forward>
bool DepthFirstPolicy<forward>::done() const
{
    return m_stack.empty();
}

template<bool forward>
NodePort DepthFirstPolicy<forward>::getCurrent()
{
    return m_stack.top();
}




template class DepthFirstPolicy<true>;
template class DepthFirstPolicy<false>;
template class Exploration<true, DepthFirstPolicy<true>>;
template class Exploration<false, DepthFirstPolicy<false>>;

}
