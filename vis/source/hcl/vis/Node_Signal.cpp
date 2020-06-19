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
