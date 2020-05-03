#include "Node.h"

#include <metaHDL_core/utils/Range.h>

namespace mhdl::vis {

Node::Node(CircuitView *circuitView) : m_circuitView(circuitView)
{
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value) 
{
    return QGraphicsItem::itemChange(change, value);
}

void Node::mousePressEvent(QGraphicsSceneMouseEvent *event) 
{
}

void Node::mouseReleaseEvent(QGraphicsSceneMouseEvent *event) 
{
}

void Node::createDefault(size_t inputs, size_t outputs)
{
    const float width = 150;
    const float heightPerPort = 20;
    const float heightPadding = 5;
    const float height = heightPerPort * std::max(inputs, outputs) + heightPadding * 2;
    
    m_background = new QGraphicsRectItem(-width/2.0f, -height/2, width, height, this);
    
    m_inputPorts.resize(inputs);
    for (auto i : utils::Range(inputs)) {
        m_inputPorts[i] = new QGraphicsItemGroup(this);
        m_inputPorts[i]->setPos(-width/2.0f, -height/2 + heightPadding + (i+0.5)*heightPerPort);
        
        new QGraphicsRectItem(-4, -9, 25.0f, 18.0f, m_inputPorts[i]);
        new QGraphicsEllipseItem(-2.5, -2.5, 5.0f, 5.0f, m_inputPorts[i]);
    }
    m_outputPorts.resize(inputs);
    for (auto i : utils::Range(outputs)) {
        m_outputPorts[i] = new QGraphicsItemGroup(this);
        m_outputPorts[i]->setPos(width/2.0f, -height/2 + heightPadding + (i+0.5)*heightPerPort);
        
        new QGraphicsRectItem(-21, -9, 25.0f, 18.0f, m_outputPorts[i]);
        new QGraphicsEllipseItem(-2.5, -2.5, 5.0f, 5.0f, m_outputPorts[i]);
    }
}



}
