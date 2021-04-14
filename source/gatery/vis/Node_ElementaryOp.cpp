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
#include "Node_ElementaryOp.h"

#include <hcl/utils/Range.h>


#include "CircuitView.h"


namespace hcl::vis {

Node_ElementaryOp::Node_ElementaryOp(CircuitView *circuitView, core::hlim::BaseNode *hlimNode) : Node(circuitView), m_hlimNode(hlimNode)
{
    m_name = m_hlimNode->getTypeName();
    
    m_inputPorts.resize(m_hlimNode->getNumInputPorts());
    for (auto i : utils::Range(m_hlimNode->getNumInputPorts())) {
        m_inputPorts[i].name = m_hlimNode->getInputName(i);
        m_inputPorts[i].producer = m_hlimNode->getDriver(i);
    }
    
    m_outputPorts.resize(m_hlimNode->getNumOutputPorts());
    for (auto i : utils::Range(m_hlimNode->getNumOutputPorts())) {
        m_outputPorts[i].name = m_hlimNode->getOutputName(i);
        m_outputPorts[i].producer = {.node = m_hlimNode, .port = i};
    }
    
    createDefaultGraphics(100);
}

}
