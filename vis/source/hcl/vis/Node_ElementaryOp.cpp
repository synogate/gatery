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
