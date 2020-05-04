#pragma once

#include <metaHDL_core/hlim/NodeIO.h>


#include <QGraphicsItem>

namespace mhdl::vis {

class CircuitView;    
    
class Node : public QGraphicsItemGroup
{
    public:
        struct Port {
            std::string name;
            QGraphicsItem *graphicsItem = nullptr;
            core::hlim::NodePort producer;
        };
        
        
        Node(CircuitView *circuitView);

        enum { Type = UserType + 1 };
        int type() const override { return Type; }

        inline const std::vector<Port> &getInputPorts() const { return m_inputPorts; }
        inline const std::vector<Port> &getOutputPorts() const { return m_outputPorts; }
                
    protected:
        CircuitView *m_circuitView;
        std::string m_name;
        QGraphicsItem *m_background = nullptr;
        QGraphicsItem *m_interior = nullptr;
        std::vector<Port> m_inputPorts;
        std::vector<Port> m_outputPorts;
        
        void createDefaultGraphics();
        
        QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
};

}
