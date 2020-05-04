#include "CircuitView.h"


#include "GraphLayouting.h"

#include "EdgeTree.h"

#include "Node_Signal.h"
#include "Node_ElementaryOp.h"
#include "Node_Entity.h"

#include <metaHDL_core/utils/Range.h>
#include <metaHDL_core/hlim/coreNodes/Node_Signal.h>
#include <metaHDL_core/hlim/coreNodes/Node_Register.h>


#include <QWheelEvent>


namespace mhdl::vis {


CircuitView::CircuitView(QWidget *parent)
    : QGraphicsView(parent) 
{
    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_scene->setSceneRect(-20000, -20000, 40000, 40000);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    m_interiorFont.setBold(true);
    m_interiorFont.setPointSizeF(5.0f);
    m_portFont.setPointSizeF(2.0f);
}



void CircuitView::render(core::hlim::Circuit &circuit, core::hlim::NodeGroup *group)
{
    m_scene->clear();
    
    std::vector<Node*> nodePtrs;
    layout::GraphLayouting layout;
    
    std::set<layout::NodePort> registerOutputs;
    
    for (auto &area : group->getChildren()) {
        for (auto &node : area->getNodes()) {
            if (dynamic_cast<core::hlim::Node_Signal*>(node) != nullptr) {
                Node_Signal *n;
                m_scene->addItem(n = new Node_Signal(this, dynamic_cast<core::hlim::Node_Signal*>(node)));
                nodePtrs.push_back(n);
            } else {
                Node_ElementaryOp *n;
                m_scene->addItem(n = new Node_ElementaryOp(this, node));
                nodePtrs.push_back(n);
                
                if (dynamic_cast<core::hlim::Node_Register*>(node) != nullptr) {
                    registerOutputs.insert({.node=nodePtrs.size()-1, .port=0ull});
                }
            }
        }
        for (auto &subEntity : area->getChildren()) {
            Node_Entity *n;
            m_scene->addItem(n = new Node_Entity(this, subEntity.get()));
            nodePtrs.push_back(n);
        }
    }
    
    std::map<core::hlim::NodePort, layout::NodePort> hlim2layout;
    for (auto n_i : utils::Range(nodePtrs.size())) {
        auto n = nodePtrs[n_i];
        
        for (auto p_i : utils::Range(n->getOutputPorts().size())) {
            auto p = n->getOutputPorts()[p_i];
            
            MHDL_ASSERT(p.producer.node != nullptr);

            hlim2layout[p.producer] = {.node = n_i, .port = p_i};
        }
    }
    
    std::map<layout::NodePort, std::vector<layout::NodePort>> edges;
    
    layout.nodes.reserve(nodePtrs.size());
    for (auto n_i : utils::Range(nodePtrs.size())) {
        auto n = nodePtrs[n_i];
        layout::Node layoutNode;
        layoutNode.width = n->childrenBoundingRect().width();
        layoutNode.height = n->childrenBoundingRect().height();
        
        for (auto p_i : utils::Range(n->getInputPorts().size())) {
            auto p = n->getInputPorts()[p_i];

            layoutNode.relativeInputPortLocations.push_back({.x = (float) p.graphicsItem->x(), .y = (float) p.graphicsItem->y()});
            
            auto it = hlim2layout.find(p.producer);
            if (it != hlim2layout.end()) 
                edges[it->second].push_back({.node=n_i, .port=p_i});
        }

        for (auto p : n->getOutputPorts())
            layoutNode.relativeOutputPortLocations.push_back({.x = (float) p.graphicsItem->x(), .y = (float) p.graphicsItem->y()});
        
        layout.nodes.push_back(layoutNode);
    }
    
    layout.edges.reserve(edges.size());
    for (const auto &p : edges) {
        layout::Edge edge;
        edge.weight = 1.0f;
        edge.src = p.first;
        edge.dst = p.second;
        
        if (registerOutputs.find(p.first) != registerOutputs.end())
            edge.weight = 0.1f;
        
        layout.edges.push_back(std::move(edge));
    }
    
    
    layout.run();
    

    for (auto i : utils::Range(nodePtrs.size())) {
        const auto &nodeLayout = layout.getNodeLayouts()[i];
        nodePtrs[i]->setPos(nodeLayout.location.x, nodeLayout.location.y);
    }
    
    for (auto i : utils::Range(layout.edges.size())) {
        m_scene->addItem(new EdgeTree(layout.getEdgeLayouts()[i]));
    }
}




void CircuitView::wheelEvent(QWheelEvent *event)
{
    scaleView(pow(2., event->angleDelta().y() / 240.0));
}

void CircuitView::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);
    /*

    QRectF sceneRect = this->sceneRect();
 
    // Text
    QRectF textRect(sceneRect.left() + 4, sceneRect.top() + 4,
                    sceneRect.width() - 4, sceneRect.height() - 4);
    QString message(tr("Awesome circuit graph goes here"));

    QFont font = painter->font();
    font.setBold(true);
    font.setPointSize(14);
    painter->setFont(font);
    painter->setPen(Qt::lightGray);
    painter->drawText(textRect.translated(2, 2), message);
    painter->setPen(Qt::black);
    painter->drawText(textRect, message);
    */
}

void CircuitView::scaleView(qreal scaleFactor)
{
    float factor = transform().scale(scaleFactor, scaleFactor).mapRect(QRectF(0, 0, 1, 1)).width();
    if (factor < 0.07 || factor > 100)
        return;

    scale(scaleFactor, scaleFactor);
}

void CircuitView::zoomIn()
{
    scaleView(qreal(1.2));
}

void CircuitView::zoomOut()
{
    scaleView(1 / qreal(1.2));
}

}
