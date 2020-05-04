#pragma once

#include "Node.h"


#include <metaHDL_core/hlim/Circuit.h>
#include <metaHDL_core/hlim/Node.h>
#include <metaHDL_core/hlim/NodeGroup.h>


#include <QGraphicsItem>
#include <QGraphicsView>

#include <vector>
#include <memory>


namespace mhdl::vis {
    


class CircuitView : public QGraphicsView
{
    Q_OBJECT
    public:
        CircuitView(QWidget *parent = nullptr);
        void render(core::hlim::Circuit &circuit, core::hlim::NodeGroup *group);

        inline const QFont &getInteriorFont() const { return m_interiorFont; }
        inline const QFont &getPortFont() const { return m_portFont; }

    public slots:
        void zoomIn();
        void zoomOut();

    protected:
        std::vector<std::unique_ptr<Node>> m_nodes;
        QGraphicsScene *m_scene;
        QFont m_interiorFont;
        QFont m_portFont;
        
        void wheelEvent(QWheelEvent *event) override;
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void scaleView(qreal scaleFactor);
};


}
