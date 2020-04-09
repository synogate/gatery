#include "NodeGroup.h"

namespace mhdl {
namespace core {
namespace hlim {

NodeGroup *NodeGroup::addChildNodeGroup()
{
    m_children.push_back(std::make_unique<NodeGroup>());
    return m_children.back().get();
}


}
}
}
