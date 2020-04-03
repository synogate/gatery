#pragma once

#include "Connection.h"
#include "ConnectionType.h"

#include <vector>
#include <string>

namespace mhdl {
namespace core {    
namespace hlim {

class Circuit;
    
class Node
{
    public:
        struct Port {
            std::string name;
            Node *node;
            ConnectionType *m_connectionType;
        };
        struct InputPort : public Port {
            Port *connection;
        };
        struct OutputPort : public Port {
            std::vector<Port *> connection;
        };

    protected:
        Circuit *m_owner;
        std::vector<InputPort> m_inputs;
        std::vector<OutputPort> m_outputs;
};

}
}
}
