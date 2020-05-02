#pragma once

#include <QGraphicsItem>
#include <QGraphicsView>


#include <metaHDL_core/hlim/Circuit.h>
#include <metaHDL_core/hlim/Node.h>
#include <metaHDL_core/hlim/NodeGroup.h>
#include <metaHDL_core/hlim/coreNodes/Node_Signal.h>


/*
class Node;

class Edge : public QGraphicsItem
{
    public:
        Edge(Node *sourceNode, Node *destNode);

        Node *sourceNode() const;
        Node *destNode() const;

        void adjust();

        enum { Type = UserType + 1 };
        int type() const override { return Type; }

    protected:
        QRectF boundingRect() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    private:
        hlim::Node_Signal *m_hlimSignal;
        
        Node *source, *dest;

        QPointF sourcePoint;
        QPointF destPoint;
};


class Node : public QGraphicsItem
{
    public:
        Node(GraphWidget *graphWidget);

        void addEdge(Edge *edge);
        QVector<Edge *> edges() const;

        enum { Type = UserType + 2 };
        int type() const override { return Type; }

        void calculateForces();
        bool advancePosition();

        QRectF boundingRect() const override;
        QPainterPath shape() const override;
        void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

        void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

    private:
        hlim::Node_Signal *m_hlimSignal;

        QVector<Edge *> edgeList;
        QPointF newPos;
        GraphWidget *graph;
};


class Group : public Node
{
public:
    Group(GraphWidget *graphWidget);

    void addEdge(Edge *edge);
    QVector<Edge *> edges() const;

    enum { Type = UserType + 1 };
    int type() const override { return Type; }

    void calculateForces();
    bool advancePosition();

    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant &value) override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;

private:
    QVector<Edge *> edgeList;
    QPointF newPos;
    GraphWidget *graph;
};
*/

class CircuitView : public QGraphicsView
{
    Q_OBJECT
    public:
        CircuitView(QWidget *parent = nullptr);

    public slots:
        void zoomIn();
        void zoomOut();

    protected:
        void wheelEvent(QWheelEvent *event) override;
        void drawBackground(QPainter *painter, const QRectF &rect) override;
        void scaleView(qreal scaleFactor);
};
