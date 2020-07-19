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
        .end = offset + int(size),
        .stride = 1,
        .untilEndOfSource = false,
    };
}

Selection Selection::StridedSlice(int offset, size_t size, int stride)
{
    return {
        .start = offset,
        .end = offset + int(size),
        .stride = stride,
        .untilEndOfSource = false,
    };
}





BVecSlice::BVecSlice(BVec *signal, const Selection &selection) : m_signal(signal), m_selection(selection)
{
}

BVecSlice::~BVecSlice() 
{ 
    if (m_signal != nullptr) 
        m_signal->unregisterSlice(this); 
}


BVecSlice &BVecSlice::operator=(const BVecSlice &slice)
{ 
    return this->operator=((BVec) slice); 
}

size_t BVecSlice::size() const
{
    HCL_ASSERT(m_signal != nullptr);
    size_t inputWidth = m_signal->getWidth();
    
    HCL_ASSERT_HINT(m_selection.stride == 1, "Strided slices not yet implemented!");
    
    size_t selectionEnd = m_selection.end;
    if (m_selection.untilEndOfSource)
        selectionEnd = inputWidth;

    HCL_DESIGNCHECK(m_selection.start >= 0);
    HCL_DESIGNCHECK((size_t) m_selection.start < inputWidth);
    
    //HCL_DESIGNCHECK(selectionEnd >= 0);
    HCL_DESIGNCHECK(selectionEnd <= inputWidth);

    return selectionEnd - m_selection.start;
}


BVecSlice &BVecSlice::operator=(const ElementarySignal &signal)
{
    HCL_ASSERT(m_signal != nullptr);
    
    
    size_t inputWidth = m_signal->getWidth();
    
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
    node->recordStackTrace();

    node->connectInput(0, m_signal->getReadPort());
    node->connectInput(1, signal.getReadPort());

    hlim::Node_Rewire::RewireOperation rewireOp;
    HCL_ASSERT_HINT(m_selection.stride == 1, "Strided slices not yet implemented!");
    
    size_t selectionEnd = m_selection.end;
    if (m_selection.untilEndOfSource)
        selectionEnd = inputWidth;

    HCL_DESIGNCHECK(m_selection.start >= 0);
    HCL_DESIGNCHECK((size_t) m_selection.start < inputWidth);
    
    //HCL_DESIGNCHECK(selectionEnd >= 0);
    HCL_DESIGNCHECK(selectionEnd <= inputWidth);

    HCL_DESIGNCHECK_HINT(selectionEnd - m_selection.start == signal.getWidth(), "When assigning a signal to a sliced signal, the widths of the assigned signal and the slicing range must match");

    
    if (m_selection.start != 0)
        rewireOp.ranges.push_back({
            .subwidth = size_t(m_selection.start),
            .source = hlim::Node_Rewire::OutputRange::INPUT,
            .inputIdx = 0,
            .inputOffset = 0,
        });

    rewireOp.ranges.push_back({
        .subwidth = selectionEnd - m_selection.start,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 1,
        .inputOffset = 0,
    });

    if (selectionEnd != inputWidth)
        rewireOp.ranges.push_back({
            .subwidth = inputWidth - selectionEnd,
            .source = hlim::Node_Rewire::OutputRange::INPUT,
            .inputIdx = 0,
            .inputOffset = selectionEnd,
        });
    
    node->setOp(std::move(rewireOp));
    node->changeOutputType(m_signal->getConnType());    
    
    m_signal->operator=(BVec(hlim::NodePort{ .node = node, .port = 0ull }));
    
    return *this;
}

BVecSlice::operator BVec() const
{
    hlim::NodePort currentInput;
    if (m_signal != nullptr) {
        currentInput = m_signal->getReadPort();
    } else
        currentInput = m_lastSignalNodePort;
    
    size_t inputWidth = currentInput.node->getOutputConnectionType(currentInput.port).width;
    
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();

    node->connectInput(0, currentInput);

    hlim::Node_Rewire::RewireOperation rewireOp;
    HCL_ASSERT_HINT(m_selection.stride == 1, "Strided slices not yet implemented!");
    
    size_t selectionEnd = m_selection.end;
    if (m_selection.untilEndOfSource)
        selectionEnd = inputWidth;

    HCL_DESIGNCHECK(m_selection.start >= 0);
    HCL_DESIGNCHECK((size_t) m_selection.start < inputWidth);
    
    //HCL_DESIGNCHECK(selectionEnd >= 0);
    HCL_DESIGNCHECK(selectionEnd <= inputWidth);
    
    rewireOp.ranges.push_back({
        .subwidth = selectionEnd - m_selection.start,
        .source = hlim::Node_Rewire::OutputRange::INPUT,
        .inputIdx = 0,
        .inputOffset = size_t(m_selection.start),
    });

    node->setOp(std::move(rewireOp));
    node->changeOutputType(currentInput.node->getOutputConnectionType(0));    
    return BVec(hlim::NodePort{ .node = node, .port = 0ull });    
}

void BVecSlice::unregisterSignal() { 
    m_lastSignalNodePort = m_signal->getReadPort();
    m_signal = nullptr;
}







BVec::BVec()
{ 
    resize(0); 
}

BVec::BVec(size_t width)
{ 
    resize(width); 
}

BVec::BVec(const hlim::NodePort &port) : ElementarySignal(port)
{ 
    
}

void BVec::resize(size_t width)
{
    HCL_DESIGNCHECK_HINT(m_node->isOrphaned(), "Can not resize signal once it is connected (driving or driven).");

    m_node->setConnectionType(getSignalType(width));
}

Bit BVec::operator[](size_t idx) const
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

void BVec::setBit(size_t idx, const Bit& in)
{
    HCL_DESIGNCHECK_HINT(getWidth() > idx, "Out of bounds vector element assignment.");

    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
    node->recordStackTrace();

    node->connectInput(0, { .node = m_node, .port = 0 });
    node->connectInput(1, in.getReadPort());

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


hlim::ConnectionType BVec::getSignalType(size_t width) const
{
    hlim::ConnectionType connectionType;
    
    connectionType.interpretation = hlim::ConnectionType::BITVEC;
    connectionType.width = width;
    
    return connectionType;
}





BVec BVec::zext(size_t width) const
{
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();

    node->connectInput(0, { .node = m_node, .port = 0 });

    hlim::Node_Rewire::RewireOperation rewireOp;
    if (width > 0 && getWidth() > 0)
    {
        rewireOp.ranges.push_back({
                .subwidth = std::min(width, getWidth()),
                .source = hlim::Node_Rewire::OutputRange::INPUT,
            });
    }

    if (width > getWidth())
    {
        rewireOp.ranges.push_back({
                .subwidth = width - getWidth(),
                .source = hlim::Node_Rewire::OutputRange::CONST_ZERO,
            });
    }

    node->setOp(std::move(rewireOp));
    node->changeOutputType(m_node->getOutputConnectionType(0));    
    return BVec(hlim::NodePort{ .node = node, .port = 0ull });
}

BVec BVec::bext(size_t width, const Bit& bit) const
{
    hlim::Node_Rewire* node = DesignScope::createNode<hlim::Node_Rewire>(2);
    node->recordStackTrace();

    node->connectInput(0, { .node = m_node, .port = 0 });
    node->connectInput(1, bit.getReadPort());

    hlim::Node_Rewire::RewireOperation rewireOp;
    if (width > 0 && getWidth() > 0)
    {
        rewireOp.ranges.push_back({
                .subwidth = std::min(width, getWidth()),
                .source = hlim::Node_Rewire::OutputRange::INPUT,
            });
    }

    if (width > getWidth())
    {
        rewireOp.ranges.resize(width - getWidth() + rewireOp.ranges.size(), {
                .subwidth = 1,
                .source = hlim::Node_Rewire::OutputRange::INPUT,
                .inputIdx = 1,
            });
    }

    node->setOp(std::move(rewireOp));
    node->changeOutputType(m_node->getOutputConnectionType(0));    
    return BVec(hlim::NodePort{.node = node, .port = 0ull});
}



}
