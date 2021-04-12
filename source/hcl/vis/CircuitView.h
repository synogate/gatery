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
#pragma once

#include "Node.h"

#include "BaseGraphicsComposite.h"

#include "Node_Signal.h"

#include <hcl/hlim/Circuit.h>
#include <hcl/hlim/Node.h>
#include <hcl/hlim/NodeGroup.h>


#include <QGraphicsItem>
#include <QGraphicsView>

#include <vector>
#include <memory>
#include <functional>


namespace hcl::vis {
    


class CircuitView : public QGraphicsView
{
    Q_OBJECT
    public:
        CircuitView(QWidget *parent = nullptr);
        void render(core::hlim::Circuit &circuit, core::hlim::NodeGroup *group, std::function<void(float)> progressCallback = std::function<void(float)>());

        inline const QFont &getInteriorFont() const { return m_interiorFont; }
        inline const QFont &getPortFont() const { return m_portFont; }
        
        virtual void mousePressEvent(QMouseEvent *event) override;
        virtual void mouseMoveEvent(QMouseEvent *event) override;

        inline const std::vector<Node*> &getNodes() const { return m_nodes; }
    public slots:
        void zoomIn();
        void zoomOut();
        
    signals:
        void onElementsClicked(const std::set<BaseGraphicsComposite*> &elements);

    protected:
        std::set<BaseGraphicsComposite*> fetchElements(int x, int y);
        
        std::set<BaseGraphicsComposite*> m_hoverItems;
               
        std::vector<Node*> m_nodes;
        
        QGraphicsScene *m_scene;
        QFont m_interiorFont;
        QFont m_portFont;
        
        void wheelEvent(QWheelEvent *event) override;
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void scaleView(qreal scaleFactor);
};


}
