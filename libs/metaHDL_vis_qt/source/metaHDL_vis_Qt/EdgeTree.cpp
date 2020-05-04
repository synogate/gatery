#include "EdgeTree.h"

namespace mhdl::vis {

EdgeTree::EdgeTree(const layout::EdgeLayout &edgeLayout)
{
    for (const auto &line : edgeLayout.lines) {
        QGraphicsLineItem *lineItem = new QGraphicsLineItem(line.from.x, line.from.y, line.to.x, line.to.y, this);
    }
}

}
