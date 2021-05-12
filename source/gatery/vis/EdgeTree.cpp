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
#include "EdgeTree.h"

#include <QBrush>
#include <QPen>

namespace hcl::vis {

EdgeTree::EdgeTree(const layout::EdgeLayout &edgeLayout)
{
    setAcceptHoverEvents(true);
    setHandlesChildEvents(true);
    
    for (const auto &line : edgeLayout.lines) {
        
        QGraphicsLineItem *lineItem = new QGraphicsLineItem(line.from.x, line.from.y, line.to.x, line.to.y, this);
        m_lines.push_back(lineItem);
    }
    for (const auto &intersection : edgeLayout.intersections) {
        QGraphicsEllipseItem *intersectionItem = new QGraphicsEllipseItem(intersection.location.x-1.5, 
                                                                          intersection.location.y-1.5,
                                                                          3,
                                                                          3, this);
        intersectionItem->setBrush(QBrush(QColor(0, 0, 0)));
        m_intersections.push_back(intersectionItem);
    }
    
}

void EdgeTree::hoverStart()
{
    for (auto line : m_lines)
        line->setPen(QPen(QColor(255, 0, 0)));
    for (auto intersection : m_intersections) {
        intersection->setBrush(QBrush(QColor(255, 0, 0)));
        intersection->setPen(QPen(QColor(255, 0, 0)));
    }
}

void EdgeTree::hoverEnd()
{
    for (auto line : m_lines)
        line->setPen(QPen(QColor(0, 0, 0)));
    for (auto intersection : m_intersections) {
        intersection->setBrush(QBrush(QColor(0, 0, 0)));
        intersection->setPen(QPen(QColor(0, 0, 0)));
    }
}


}
