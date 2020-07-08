#include "BitVector.h"

namespace hcl::core::frontend {
    
    
Selection Selection::From(int start)
{
    return {
        .start = start,
        .end = 0,
        .stride = 1,
        .untilEndOfSource = true,
    };
}

Selection Selection::Range(int start, int end)
{
    return {
        .start = start,
        .end = end,
        .stride = 1,
        .untilEndOfSource = false,
    };
}

Selection Selection::RangeIncl(int start, int endIncl)
{
    return {
        .start = start,
        .end = endIncl+1,
        .stride = 1,
        .untilEndOfSource = false,
    };
}

Selection Selection::StridedRange(int start, int end, int stride)
{
    return {
        .start = start,
        .end = end,
        .stride = stride,
        .untilEndOfSource = false,
    };
}

Selection Selection::Slice(int offset, size_t size)
{
    return {
        .start = offset,
        .end = offset+size,
        .stride = 1,
        .untilEndOfSource = false,
    };
}

Selection Selection::StridedSlice(int offset, size_t size, int stride)
{
    return {
        .start = offset,
        .end = offset+size,
        .stride = stride,
        .untilEndOfSource = false,
    };
}
    
    
void ElementaryVector::resize(size_t width)
{
    HCL_DESIGNCHECK_HINT(m_node->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");

    m_node->setConnectionType(getSignalType(width));
}

Bit ElementaryVector::operator[](size_t idx) const
{
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();

    size_t w = m_node->getOutputConnectionType(0).width;
    idx = (w + (idx % w)) % w;

    hlim::Node_Rewire::RewireOperation rewireOp;
    rewireOp.ranges.push_back({
        .subwidth = 1,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 0,
        .inputOffset = (size_t)idx,
        });
    node->setOp(std::move(rewireOp));

    node->connectInput(0, { .node = m_node, .port = 0ull });
    node->changeOutputType({.interpretation = hlim::ConnectionType::BOOL});

    return Bit({ .node = node, .port = 0ull });
}

void ElementaryVector::setBit(size_t idx, const Bit& in)
{
    HCL_DESIGNCHECK_HINT(getWidth() > idx, "Out of bounds vector element assignment.");

    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
    node->recordStackTrace();

    node->connectInput(0, { .node = m_node, .port = 0 });
    node->connectInput(1, { .node = in.getNode(), .port = 0 });

    hlim::Node_Rewire::RewireOperation rewireOp;
    if (idx != 0)
        rewireOp.ranges.push_back({
            .subwidth = idx,
            .source = hlim::Node_Rewire::OutputRange::INPUT,
            .inputIdx = 0,
            .inputOffset = 0,
        });

    rewireOp.ranges.push_back({
        .subwidth = 1,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 1,
        .inputOffset = 0,
    });

    if (idx + 1 != getWidth())
    {
        rewireOp.ranges.push_back({
            .subwidth = getWidth() - idx - 1,
            .source = hlim::Node_Rewire::OutputRange::INPUT,
            .inputIdx = 0,
            .inputOffset = idx + 1,
        });
    }

    node->setOp(std::move(rewireOp));
    node->changeOutputType(m_node->getOutputConnectionType(0));    

    m_node = DesignScope::createNode<hlim::Node_Signal>();
    m_node->recordStackTrace();
    m_node->setConnectionType(node->getOutputConnectionType(0));
    m_node->connectInput({ .node = node, .port = 0 });
}

BitVector::BitVector(size_t width)
{ 
    resize(width); 
}

BitVector::BitVector(const hlim::NodePort &port) : BaseBitVector<BitVector>(port)
{ 
    
}

hlim::ConnectionType BitVector::getSignalType(size_t width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::RAW;
    connectionType.width = width;
    
    return connectionType;
}


}
