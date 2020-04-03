#pragma once

#include "ConnectionType.h"

#include <vector>

namespace mhdl {
namespace core {    
namespace hlim {
  

class Node;
    
/**
 * @todo write docs
 */
class Connection
{
    public:
        
    protected:
        Node *m_source;
        std::vector<Node*> m_sinks;
};

}
}
}
