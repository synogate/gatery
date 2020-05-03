#pragma once

#include <QGraphicsItem>

namespace mhdl::vis {

class CircuitView;    
    
class Node : public QGraphicsItemGroup
{
    public:
        Node(CircuitView *circuitView);

        enum { Type = UserType + 1 };
        int type() const override { return Type; }
    protected:
        CircuitView *m_circuitView;
        
        QGraphicsItem *m_background = nullptr;
        QGraphicsItem *m_interior = nullptr;
        std::vector<QGraphicsItem*> m_inputPorts;
        std::vector<QGraphicsItem*> m_outputPorts;
        
        void createDefault(size_t inputs, size_t outputs);
        
        QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
};

}
