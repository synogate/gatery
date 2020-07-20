#include "Bit.h"
#include "BitVector.h"

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

    Bit::Bit() :
        ElementarySignal{getSignalType(1), ElementarySignal::InitUnconnected::x}
    {}

    Bit::Bit(const Bit &rhs) : ElementarySignal(rhs, ElementarySignal::InitCopyCtor::x) 
    {
    }

    Bit::Bit(const hlim::NodePort &port) : ElementarySignal(port, ElementarySignal::InitOperation::x)
    { 
        setConnectionType(getSignalType(1));    
    }

    Bit::Bit(bool value) : Bit(bool2bit(value)) {
    }
    
    Bit::Bit(const Bit &rhs, ElementarySignal::InitSuccessor) : ElementarySignal(rhs, ElementarySignal::InitSuccessor::x) 
    {
    }
    

    hlim::ConnectionType Bit::getSignalType(size_t width) const 
    {
        HCL_ASSERT(width == 1);
    
        hlim::ConnectionType connectionType;
    
        connectionType.interpretation = hlim::ConnectionType::BOOL;
        connectionType.width = 1;
    
        return connectionType;
    }

    BVec Bit::zext(size_t width) const 
    {
        hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(1);
        node->recordStackTrace();

        node->connectInput(0, getReadPort());

        hlim::Node_Rewire::RewireOperation rewireOp;
        if (width > 0)
        {
            rewireOp.ranges.push_back({
                    .subwidth = 1,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 0,
                    .inputOffset = 0,
                });
        }

        if (width > 1)
        {
            rewireOp.ranges.push_back({
                    .subwidth = width - 1,
                    .source = hlim::Node_Rewire::OutputRange::CONST_ZERO,
                });
        }

        node->setOp(std::move(rewireOp));
        node->changeOutputType({.interpretation = hlim::ConnectionType::BITVEC});
        return BVec(hlim::NodePort{ .node = node, .port = 0ull });
    }
    
    BVec Bit::sext(size_t width) const 
    { 
        return bext(width, *this);
    }
    
    BVec Bit::bext(size_t width, const Bit& bit) const
    {
        hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
        node->recordStackTrace();

        node->connectInput(0, getReadPort());
        node->connectInput(1, bit.getReadPort());

        hlim::Node_Rewire::RewireOperation rewireOp;
        if (width > 0)
        {
            rewireOp.ranges.push_back({
                    .subwidth = 1,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 0,
                    .inputOffset = 0,
                });
        }

        if (width > 1)
        {
            rewireOp.ranges.resize(width - 1 + rewireOp.ranges.size(), {
                    .subwidth = 1,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 1,
                    .inputOffset = 0,
                });
        }

        node->setOp(std::move(rewireOp));
        node->changeOutputType({.interpretation = hlim::ConnectionType::BITVEC});    
        return BVec(hlim::NodePort{.node = node, .port = 0ull});
    }

    Bit& Bit::operator=(bool value)
    {
        assign(bool2bit(value));
        return *this;
    }
    
    const Bit Bit::operator*() const
    {
        return Bit(*this, ElementarySignal::InitSuccessor::x);
    }
    
}
