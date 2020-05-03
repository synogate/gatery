#include "CircuitView.h"

#include "Node_Signal.h"

#include <QWheelEvent>


namespace mhdl::vis {


CircuitView::CircuitView(QWidget *parent)
    : QGraphicsView(parent) 
{
    m_scene = new QGraphicsScene(this);
    m_scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    m_scene->setSceneRect(-2000, -2000, 4000, 4000);
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
    setDragMode(ScrollHandDrag);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}



void CircuitView::render(core::hlim::Circuit &circuit, core::hlim::NodeGroup *group)
{
    m_scene->clear();
    
    float offset = 0.0f;
    for (auto &area : group->getChildren())
        for (auto &node : area->getNodes())
            if (dynamic_cast<core::hlim::Node_Signal*>(node) != nullptr) {
                Node_Signal *n;
                m_scene->addItem(n = new Node_Signal(this, dynamic_cast<core::hlim::Node_Signal*>(node)));
                n->setPos(0.0f, offset);
                offset += 40;
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
