#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"
#include "Scope.h"

#include "../hlim/coreNodes/Node_Signal.h"
#include "../hlim/coreNodes/Node_Rewire.h"

#include "../utils/Preprocessor.h"
#include "../utils/Traits.h"


#include <boost/format.hpp>

#include <optional>


namespace mhdl::core::frontend {
    
class SignalBitShiftOp
{
    public:
        SignalBitShiftOp(int shift) : m_shift(shift) { }
        
        inline SignalBitShiftOp &setFillLeft(bool bit) { m_fillLeft = bit; return *this; }
        inline SignalBitShiftOp &setFillRight(bool bit) { m_fillRight = bit; return *this; }
        inline SignalBitShiftOp &duplicateLeft() { m_duplicateLeft = true; m_rotate = false; return *this; }
        inline SignalBitShiftOp &duplicateRight() { m_duplicateRight = true; m_rotate = false; return *this; }
        inline SignalBitShiftOp &rotate() { m_rotate = true; m_duplicateLeft = m_duplicateRight = false; return *this; }
        
        hlim::ConnectionType getResultingType(const hlim::ConnectionType &operand);
        
        template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>
        SignalType operator()(const SignalType &operand);
    protected:
        int m_shift;
        bool m_duplicateLeft = false;
        bool m_duplicateRight = false;
        bool m_rotate = false;
        bool m_fillLeft = false;
        bool m_fillRight = false;
};


template<typename SignalType, typename>
SignalType SignalBitShiftOp::operator()(const SignalType &operand) {
    hlim::Node_Signal *signal = operand.getNode();
    MHDL_ASSERT(signal != nullptr);
    
    hlim::Node_Rewire *node = DesignScope::createNode<hlim::Node_Rewire>(1);
    node->recordStackTrace();
    
    size_t absShift = std::abs(m_shift);
    
    hlim::Node_Rewire::RewireOperation rewireOp;
    if (m_rotate) {
        MHDL_ASSERT_HINT(false, "Not implemented yet!");
    } else {
        if (m_shift < 0) {            
            if (absShift < signal->getOutputConnectionType(0).width) {
                rewireOp.ranges.push_back({
                    .subwidth = signal->getOutputConnectionType(0).width - absShift,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 0,
                    .inputOffset = (size_t) absShift,
                });
            }
            
            if (m_duplicateLeft) {
                for (size_t i = 0; i < (size_t) absShift; i++) {
                    rewireOp.ranges.push_back({
                        .subwidth = 1,
                        .source = hlim::Node_Rewire::OutputRange::INPUT,
                        .inputIdx = 0,
                        .inputOffset = signal->getOutputConnectionType(0).width-1,
                    });
                }
            } else {
                rewireOp.ranges.push_back({
                    .subwidth = absShift,
                    .source = (m_fillLeft? hlim::Node_Rewire::OutputRange::CONST_ONE : hlim::Node_Rewire::OutputRange::CONST_ZERO),
                });
            }
        } else {
            if (m_duplicateRight) {
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
            if (absShift < signal->getOutputConnectionType(0).width) {
                rewireOp.ranges.push_back({
                    .subwidth = signal->getOutputConnectionType(0).width - absShift,
                    .source = hlim::Node_Rewire::OutputRange::INPUT,
                    .inputIdx = 0,
                    .inputOffset = 0,
                });
            }
        }
    }
    node->setOp(std::move(rewireOp));
    node->connectInput(0, {.node = signal, .port = 0});
    return SignalType({.node = node, .port = 0});
}





template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>
SignalType operator<<(const SignalType &signal, int amount)  {                                 
    MHDL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
    SignalBitShiftOp op((int) amount);                                                              
    return op(signal);                                                                              
}

template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>
SignalType operator>>(const SignalType &signal, int amount)  {                                 
    MHDL_DESIGNCHECK_HINT(amount >= 0, "Shifting by negative amount not allowed!");
    SignalBitShiftOp op(- (int)amount);                                                              
    if (utils::isSignedIntegerSignal<SignalType>::value)
        op.duplicateLeft();
    return op(signal);                                                                              
}

template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>
SignalType &operator<<=(SignalType &signal, int amount)  {
    signal = signal << amount;
    return signal; 
}

template<typename SignalType, typename = std::enable_if_t<utils::isBitVectorSignal<SignalType>::value>>
SignalType &operator>>=(SignalType &signal, int amount)  {
    signal = signal >> amount;
    return signal;
}


}
