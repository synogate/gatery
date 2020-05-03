#include "Node_Signal.h"


namespace mhdl::vis {

Node_Signal::Node_Signal(CircuitView *circuitView, core::hlim::Node_Signal *hlimNode) : Node(circuitView), m_hlimNode(hlimNode)
{
    createDefault(1, 1);
    if (!m_hlimNode->getName().empty()) {
        QGraphicsTextItem *text;
        m_interior = text = new QGraphicsTextItem(QString::fromUtf8(m_hlimNode->getName().c_str()), this);
        
        text->setTextWidth(100);
        text->adjustSize();
        text->setPos(-50.0f, -text->boundingRect().height()/2);
    }
}

}
