#include "CircuitView.h"


#include "GraphLayouting.h"

#include "EdgeTree.h"

#include "Node_Signal.h"
#include "Node_ElementaryOp.h"
#include "Node_Entity.h"

#include <hcl/utils/Range.h>
#include <hcl/hlim/coreNodes/Node_Signal.h>
#include <hcl/hlim/coreNodes/Node_Register.h>


#include <QWheelEvent>

#include <iostream>

namespace hcl::vis {


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


void CircuitView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::RightButton) {
        std::set<BaseGraphicsComposite*> clickedList = fetchElements(event->x(), event->y());
        
        onElementsClicked(clickedList);
    }
    
    QGraphicsView::mousePressEvent(event);
}

void CircuitView::mouseMoveEvent(QMouseEvent *event) 
{
    std::set<BaseGraphicsComposite*> newHoverList = fetchElements(event->x(), event->y());
    
    for (auto p : m_hoverItems) {
        if (newHoverList.find(p) == newHoverList.end())
            p->hoverEnd();
    }
    for (auto p : newHoverList) {
        if (m_hoverItems.find(p) == m_hoverItems.end())
            p->hoverStart();
    }
    
    std::swap(m_hoverItems, newHoverList);
    
    QGraphicsView::mouseMoveEvent(event);
}

std::set<BaseGraphicsComposite*> CircuitView::fetchElements(int x, int y)
{
    auto list = items(x, y);
    
    std::set<BaseGraphicsComposite*> elements;
    for (auto p : list) {
        while (p != nullptr) {
            BaseGraphicsComposite *cast_p = dynamic_cast<BaseGraphicsComposite*>(p);
            if (cast_p != nullptr) {
                elements.insert(cast_p);
                break;
            }
            p = p->parentItem();
        }
    }
    return elements;
}

void CircuitView::render(core::hlim::Circuit &circuit, core::hlim::NodeGroup *group, std::function<void(float)> progressCallback)
{
    m_scene->clear();

    m_nodes.clear();
    
    layout::GraphLayouting layout;
    
    std::set<layout::NodePort> registerOutputs;
    
    std::vector<core::hlim::NodeGroup*> groupStack = { group };
    
    while (!groupStack.empty()) {
        core::hlim::NodeGroup *nodeGroup = groupStack.back();
        groupStack.pop_back();
        
        for (auto &subGroup : nodeGroup->getChildren()) {
            if (subGroup->getGroupType() == core::hlim::NodeGroup::GroupType::ENTITY || subGroup->getGroupType() == core::hlim::NodeGroup::GroupType::SFU) {
                Node_Entity *n;
                m_scene->addItem(n = new Node_Entity(this, subGroup.get()));
                m_nodes.push_back(n);
            } else
                groupStack.push_back(subGroup.get());
        }
        for (auto &node : nodeGroup->getNodes()) {
            if (dynamic_cast<core::hlim::Node_Signal*>(node) != nullptr) {
                Node_Signal *n;
                m_scene->addItem(n = new Node_Signal(this, dynamic_cast<core::hlim::Node_Signal*>(node)));
                m_nodes.push_back(n);
            } else {
                Node_ElementaryOp *n;
                m_scene->addItem(n = new Node_ElementaryOp(this, node));
                m_nodes.push_back(n);
                
                for (auto i : utils::Range(node->getNumInputPorts())) {
                    auto driver = node->getDriver(i);
                    if (driver.node != nullptr)
                        if (driver.node->getGroup() == nullptr || (driver.node->getGroup() != group && !driver.node->getGroup()->isChildOf(group))) {
                            ///@todo: Mark as input
                            if (dynamic_cast<core::hlim::Node_Signal*>(driver.node) != nullptr) {
                                Node_Signal *n;
                                m_scene->addItem(n = new Node_Signal(this, dynamic_cast<core::hlim::Node_Signal*>(driver.node)));
                                m_nodes.push_back(n);
                            } else {
                                Node_ElementaryOp *n;
                                m_scene->addItem(n = new Node_ElementaryOp(this, driver.node));
                                m_nodes.push_back(n);
                            }
                        }
                }
                
                if (dynamic_cast<core::hlim::Node_Register*>(node) != nullptr) {
                    registerOutputs.insert({.node=m_nodes.size()-1, .port=0ull});
                }
            }
        }
    }
    
    std::map<core::hlim::NodePort, layout::NodePort> hlim2layout;
    for (auto n_i : utils::Range(m_nodes.size())) {
        auto n = m_nodes[n_i];
        
        for (auto p_i : utils::Range(n->getOutputPorts().size())) {
            auto p = n->getOutputPorts()[p_i];
            
            HCL_ASSERT(p.producer.node != nullptr);

            hlim2layout[p.producer] = {.node = n_i, .port = p_i};
        }
    }
    
    std::map<layout::NodePort, std::vector<layout::NodePort>> edges;
    
    layout.nodes.reserve(m_nodes.size());
    for (auto n_i : utils::Range(m_nodes.size())) {
        auto n = m_nodes[n_i];
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
    
    
    layout.run(progressCallback);
    

    for (auto i : utils::Range(m_nodes.size())) {
        const auto &nodeLayout = layout.getNodeLayouts()[i];
        m_nodes[i]->setPos(nodeLayout.location.x, nodeLayout.location.y);
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
