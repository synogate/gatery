#include "NodeGroup.h"

namespace mhdl {
namespace core {
namespace hlim {

NodeGroup *NodeGroup::addChildNodeGroup()
{
    m_children.push_back(std::make_unique<NodeGroup>());
    m_children.back()->m_parent = this;
    return m_children.back().get();
}


}
}
}
