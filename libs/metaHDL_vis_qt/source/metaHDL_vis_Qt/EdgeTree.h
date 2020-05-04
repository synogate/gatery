#pragma once

#include "GraphLayouting.h"

#include <QGraphicsItemGroup>

#include <vector>
#include <memory>

namespace mhdl::vis {

class EdgeTree : public QGraphicsItemGroup
{
    public:
        EdgeTree(const layout::EdgeLayout &edgeLayout);

        enum { Type = UserType + 3 };
        int type() const override { return Type; }
    private:
};

}
