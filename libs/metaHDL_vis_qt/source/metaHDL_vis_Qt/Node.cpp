#include "Node.h"

#include "CircuitView.h"


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

void Node::createDefaultGraphics()
{
    const float width = 150;
    const float heightPerPort = 20;
    const float heightPadding = 5;
    const float height = heightPerPort * std::max(m_inputPorts.size(), m_outputPorts.size()) + heightPadding * 2;
    
    m_background = new QGraphicsRectItem(-width/2.0f, -height/2, width, height, this);
    

    for (auto i : utils::Range(m_inputPorts.size())) {
        m_inputPorts[i].graphicsItem = new QGraphicsItemGroup(this);
        m_inputPorts[i].graphicsItem->setPos(-width/2.0f, -height/2 + heightPadding + (i+0.5)*heightPerPort);
        
        new QGraphicsRectItem(-4, -9, 25.0f, 18.0f, m_inputPorts[i].graphicsItem);
        new QGraphicsEllipseItem(-2.5, -2.5, 5.0f, 5.0f, m_inputPorts[i].graphicsItem);

        if (!m_inputPorts[i].name.empty()) {
            std::string name = m_inputPorts[i].name;
            if (name.length() > 10)
                name = name.substr(0, 7) + "...";
            
            QGraphicsTextItem *text = new QGraphicsTextItem(QString::fromUtf8(name.c_str()), m_inputPorts[i].graphicsItem);
            text->setFont(m_circuitView->getPortFont());
            text->adjustSize();
            text->setPos(1.0f, -text->boundingRect().height()/2);            
        }
    }

    for (auto i : utils::Range(m_outputPorts.size())) {
        m_outputPorts[i].graphicsItem = new QGraphicsItemGroup(this);
        m_outputPorts[i].graphicsItem->setPos(width/2.0f, -height/2 + heightPadding + (i+0.5)*heightPerPort);
        
        new QGraphicsRectItem(-21, -9, 25.0f, 18.0f, m_outputPorts[i].graphicsItem);
        new QGraphicsEllipseItem(-2.5, -2.5, 5.0f, 5.0f, m_outputPorts[i].graphicsItem);

        if (!m_outputPorts[i].name.empty()) {
            std::string name = m_outputPorts[i].name;
            if (name.length() > 10)
                name = name.substr(0, 7) + "...";
            
            QGraphicsTextItem *text = new QGraphicsTextItem(QString::fromUtf8(name.c_str()), m_outputPorts[i].graphicsItem);
            text->setFont(m_circuitView->getPortFont());
            text->adjustSize();
            text->setPos(-1.0f-text->boundingRect().width(), -text->boundingRect().height()/2);            
        }
    }
    
    if (!m_name.empty()) {
        QGraphicsTextItem *text;
        m_interior = text = new QGraphicsTextItem(QString::fromUtf8(m_name.c_str()), this);
        text->setFont(m_circuitView->getInteriorFont());
        text->setTextWidth(100);
        text->adjustSize();
        text->setPos(-50.0f, -text->boundingRect().height()/2);
    }
}



}
