#pragma once

#include "../hlim/Node.h"

namespace mhdl {
namespace core {    
namespace frontend {

class BaseSignal {
    public:
        BaseSignal();
    protected:
        hlim::Circuit *m_circuit;
        hlim::Node::OutputPort *m_port;
};
    
template<typename FinalType>
class Signal : public BaseSignal {
    public:
        virtual void assign(const FinalType &rhs) = 0;
    protected:
        
};    
    
}
}
}
