#include "SignalBitshiftOp.h"

namespace hcl::core::frontend {

hlim::ConnectionType SignalBitShiftOp::getResultingType(const hlim::ConnectionType &operand) {
    return operand;
}


BVec SignalBitShiftOp::operator()(const BVec &operand) {
    const size_t width = operand.size();
    
    hlim::Node_Rewire *node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();
    
    node->changeOutputType(operand.getConnType());
    
    size_t absShift = std::abs(m_shift);
    
    hlim::Node_Rewire::RewireOperation rewireOp;
    if (m_shift < 0) {            
        if (absShift < width) {
            rewireOp.ranges.push_back({
                .subwidth = width - absShift,
                .source = hlim::Node_Rewire::OutputRange::INPUT,
                .inputIdx = 0,
                .inputOffset = (size_t) absShift,
            });
        }
            
        if (m_rotate) {
            rewireOp.ranges.push_back({
                .subwidth = absShift,
                .source = hlim::Node_Rewire::OutputRange::INPUT,
                .inputIdx = 0,
                .inputOffset = 0
            });
        } else if (m_duplicateLeft) {
            for (size_t i = 0; i < (size_t) absShift; i++) {
                rewireOp.ranges.push_back({
                    .subwidth = 1,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 0,
                    .inputOffset = width -1,
                });
            }
        } else {
            rewireOp.ranges.push_back({
                .subwidth = absShift,
                .source = (m_fillLeft? hlim::Node_Rewire::OutputRange::CONST_ONE : hlim::Node_Rewire::OutputRange::CONST_ZERO),
            });
        }
    } else {
        if (m_rotate) {
            rewireOp.ranges.push_back({
                .subwidth = absShift,
                .source = hlim::Node_Rewire::OutputRange::INPUT,
                .inputIdx = 0,
                .inputOffset = width - absShift
            });
        } else if (m_duplicateRight) {
            for (size_t i = 0; i < (size_t) absShift; i++) {
                rewireOp.ranges.push_back({
                    .subwidth = 1,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 0,
                    .inputOffset = 0,
                });
            }
        } else {
            rewireOp.ranges.push_back({
                .subwidth = absShift,
                .source = (m_fillLeft? hlim::Node_Rewire::OutputRange::CONST_ONE : hlim::Node_Rewire::OutputRange::CONST_ZERO),
            });
        }
        if (absShift < width) {
            rewireOp.ranges.push_back({
                .subwidth = width - absShift,
                .source = hlim::Node_Rewire::OutputRange::INPUT,
                .inputIdx = 0,
                .inputOffset = 0,
            });
        }
    }

    node->setOp(std::move(rewireOp));
    node->connectInput(0, operand.getReadPort());
    return SignalReadPort(node);
}




BVec operator<<(const BVec &signal, int amount)  {                                 
    HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
    SignalBitShiftOp op((int) amount);                                                              
    return op(signal);                                                                              
}

BVec operator>>(const BVec &signal, int amount)  {                                 
    HCL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
    SignalBitShiftOp op(- (int)amount);                                                              
//    if (utils::isSignedIntegerSignal<BVec>::value)
//        op.duplicateLeft();
    return op(signal);                                                                              
}

BVec &operator<<=(BVec &signal, int amount)  {
    signal = signal << amount;
    return signal; 
}

BVec &operator>>=(BVec &signal, int amount)  {
    signal = signal >> amount;
    return signal;
}

BVec rot(const BVec& signal, int amount)
{
    SignalBitShiftOp op((int)amount);
    op.rotate();
    return op(signal);
}

}
