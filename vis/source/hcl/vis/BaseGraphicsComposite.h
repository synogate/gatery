#pragma once

#include <QGraphicsItemGroup>

namespace hcl::vis {
    

class BaseGraphicsComposite : public QGraphicsItemGroup
{
    public:
        virtual void hoverStart() { }
        virtual void hoverEnd() { }
    protected:
};

}
