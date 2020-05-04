#include "EdgeTree.h"

#include <QBrush>
#include <QPen>

namespace mhdl::vis {

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
