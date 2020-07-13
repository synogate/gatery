#include "Bit.h"

#include <hcl/hlim/coreNodes/Node_Constant.h>

namespace hcl::core::frontend {

    static Bit bool2bit(bool value)
    {
        hlim::ConnectionType type{
            .interpretation = hlim::ConnectionType::BOOL,
            .width = 1
        };

        hlim::ConstantData cv;
        cv.base = 2;
        cv.bitVec.push_back(value);

        auto* node = DesignScope::createNode<hlim::Node_Constant>(cv, type);
        return Bit({ .node = node, .port = 0ull });
    }

    Bit::Bit()
    {
        HCL_ASSERT(m_node->isOrphaned());
        m_node->setConnectionType(getSignalType(1));
    }

    Bit::Bit(const Bit &rhs) : ElementarySignal(rhs) {
        assign(rhs);
        m_node->setConnectionType(getSignalType(1));
    }

    Bit::Bit(const hlim::NodePort &port) : ElementarySignal(port) 
    { 
        m_node->setConnectionType(getSignalType(1));    
    }

    Bit::Bit(bool value) : Bit(bool2bit(value)) {
    }

    hlim::ConnectionType Bit::getSignalType(size_t width) const 
    {
        HCL_ASSERT(width == 1);
    
        hlim::ConnectionType connectionType;
    
        connectionType.interpretation = hlim::ConnectionType::BOOL;
        connectionType.width = 1;
    
        return connectionType;
    }

    Bit& Bit::operator=(bool value)
    {
        assign(bool2bit(value));
        return *this;
    }

}
