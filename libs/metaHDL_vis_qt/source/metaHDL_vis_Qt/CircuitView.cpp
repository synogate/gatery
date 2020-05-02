#include "CircuitView.h"

#include <QWheelEvent>

CircuitView::CircuitView(QWidget *parent)
    : QGraphicsView(parent) 
{
    QGraphicsScene *scene = new QGraphicsScene(this);
    scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    scene->setSceneRect(-200, -200, 400, 400);
    setScene(scene);
    setRenderHint(QPainter::Antialiasing);
    setTransformationAnchor(AnchorUnderMouse);
}



void CircuitView::wheelEvent(QWheelEvent *event)
{
    scaleView(pow(2., event->angleDelta().y() / 240.0));
}

void CircuitView::drawBackground(QPainter *painter, const QRectF &rect)
{
    Q_UNUSED(rect);

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
