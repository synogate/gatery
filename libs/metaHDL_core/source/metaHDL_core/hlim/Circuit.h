#pragma once

#include "Node.h"
#include "ConnectionType.h"

#include <vector>
#include <memory>

namespace mhdl {
namespace core {    
namespace hlim {


class Circuit : public Node
{
    public:
        
    protected:
        std::vector<std::unique_ptr<ConnectionType>> m_connectionTypes;
        std::vector<std::unique_ptr<Node>> m_nodes;
};


}
}
}
