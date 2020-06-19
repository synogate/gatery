#pragma once

#include "BaseGraphicsComposite.h"

#include "GraphLayouting.h"

#include <QGraphicsItemGroup>

#include <vector>
#include <memory>

namespace hcl::vis {

class EdgeTree : public BaseGraphicsComposite
{
    public:
        EdgeTree(const layout::EdgeLayout &edgeLayout);

        enum { Type = UserType + 3 };
        int type() const override { return Type; }
        
        virtual void hoverStart() override;
        virtual void hoverEnd() override;
    private:
        std::vector<QGraphicsLineItem *> m_lines;
        std::vector<QGraphicsEllipseItem *> m_intersections;
        
};

}
