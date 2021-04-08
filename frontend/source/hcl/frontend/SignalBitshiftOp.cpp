/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "SignalBitshiftOp.h"

#include <hcl/hlim/coreNodes/Node_Shift.h>

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

static BVec internal_shift(const BVec& signal, const BVec& amount, hlim::Node_Shift::dir direction, hlim::Node_Shift::fill fill)
{
    hlim::Node_Shift* node = DesignScope::createNode<hlim::Node_Shift>(direction, fill);
    node->recordStackTrace();

    node->connectOperand(signal.getReadPort());
    node->connectAmount(amount.getReadPort());
    return SignalReadPort(node);
}

BVec zshl(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::zero);
}

BVec oshl(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::one);
}

BVec sshl(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::last);
}

BVec zshr(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::zero);
}

BVec oshr(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::one);
}

BVec sshr(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::last);
}

BVec rotl(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::left, hlim::Node_Shift::fill::rotate);
}

BVec rotr(const BVec& signal, const BVec& amount)
{
    return internal_shift(signal, amount, hlim::Node_Shift::dir::right, hlim::Node_Shift::fill::rotate);
}

}
