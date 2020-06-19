#pragma once

#include <QGraphicsItemGroup>

namespace mhdl::vis {
    

class BaseGraphicsComposite : public QGraphicsItemGroup
{
    public:
        virtual void hoverStart() { }
        virtual void hoverEnd() { }
    protected:
};

}
