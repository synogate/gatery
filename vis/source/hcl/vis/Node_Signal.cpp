/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "Node_Signal.h"

#include "CircuitView.h"


namespace hcl::vis {

Node_Signal::Node_Signal(CircuitView *circuitView, core::hlim::Node_Signal *hlimNode) : Node(circuitView), m_hlimNode(hlimNode)
{
    m_name = m_hlimNode->getName();
    
    m_inputPorts.resize(1);
    m_inputPorts[0].producer = m_hlimNode->getDriver(0);
    m_outputPorts.resize(1);
    m_outputPorts[0].producer = {.node = m_hlimNode, .port = 0ull};
    
    if (m_name.empty())
        createDefaultGraphics(50);
    else
        createDefaultGraphics(50 + m_name.size() * 5.0f);
    
    dynamic_cast<QGraphicsRectItem*>(m_background)->setBrush(QBrush(QColor(128, 200, 128)));    
}


}
