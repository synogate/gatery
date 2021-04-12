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
#include "Node.h"

#include "CircuitView.h"


#include <hcl/utils/Range.h>

namespace hcl::vis {

Node::Node(CircuitView *circuitView) : m_circuitView(circuitView)
{
    /*
    setAcceptHoverEvents(true);
    setHandlesChildEvents(true);
    */
}

QVariant Node::itemChange(GraphicsItemChange change, const QVariant &value) 
{
    return QGraphicsItemGroup::itemChange(change, value);
}


void Node::createDefaultGraphics(float width)
{
    const float heightPerPort = 10;
    const float heightPadding = 5;
    const float height = heightPerPort * std::max(m_inputPorts.size(), m_outputPorts.size()) + heightPadding * 2;
    
    m_background = new QGraphicsRectItem(-width/2.0f, -height/2, width-1, height-1, this);
    

    for (auto i : utils::Range(m_inputPorts.size())) {
        m_inputPorts[i].graphicsItem = new QGraphicsItemGroup(this);
        m_inputPorts[i].graphicsItem->setPos(-width/2.0f, -height/2 + heightPadding + (i+0.5)*heightPerPort);
        
        QGraphicsRectItem *rect = new QGraphicsRectItem(-4, -4, 25.0f, 8.0f, m_inputPorts[i].graphicsItem);
        rect->setBrush(QBrush(QColor(128, 128, 255)));
        QGraphicsEllipseItem *ellipse = new QGraphicsEllipseItem(-2.5, -2.5, 5.0f, 5.0f, m_inputPorts[i].graphicsItem);
        ellipse->setBrush(QBrush(QColor(255, 128, 128)));

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
        
        QGraphicsRectItem *rect = new QGraphicsRectItem(-21, -4, 25.0f, 8.0f, m_outputPorts[i].graphicsItem);
        rect->setBrush(QBrush(QColor(128, 128, 255)));
        QGraphicsEllipseItem *ellipse = new QGraphicsEllipseItem(-2.5, -2.5, 5.0f, 5.0f, m_outputPorts[i].graphicsItem);
        ellipse->setBrush(QBrush(QColor(255, 128, 128)));

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
        text->setTextWidth(width-50);
        text->adjustSize();
        text->setPos(-text->boundingRect().width()/2, -text->boundingRect().height()/2);
    }
}



}
