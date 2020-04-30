/*
 * <one line to give the program's name and a brief idea of what it does.>
 * Copyright (C) 2020  <copyright holder> <email>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "uart.h"

#include <metaHDL_core/frontend/Constant.h>
#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/frontend/SignalArithmeticOp.h>
#include <metaHDL_core/frontend/SignalLogicOp.h>
#include <metaHDL_core/frontend/SignalBitshiftOp.h>
#include <metaHDL_core/frontend/SignalMiscOp.h>
#include <metaHDL_core/frontend/PriorityConditional.h>

#include <metaHDL_core/utils/Exceptions.h>

using namespace mhdl::core::frontend;
using namespace mhdl::core::hlim;


UartTransmitter::UartTransmitter(size_t dataBits, size_t stopBits, size_t clockCyclesPerBit) :
            m_dataBits(dataBits), m_stopBits(stopBits), m_clockCyclesPerBit(clockCyclesPerBit) 
{
}
        
void UartTransmitter::operator()(const BitVector &inputData, Bit send, Bit &outputLine, Bit &idle)
{
    MHDL_NAMED(send);
    MHDL_NAMED(outputLine);
    MHDL_NAMED(idle);


    GroupScope entity(NodeGroup::GRP_ENTITY);
    entity.setName("UartTransmitter");
    
    GroupScope area(NodeGroup::GRP_AREA);
    area.setName("all");
    
    // todo: constants!
    Bit enable = 1_bit;
    MHDL_NAMED(enable);

    RegisterFactory reg({}, {});
    
    UnsignedInteger bitCounter(4);
    MHDL_NAMED(bitCounter);
    
    BitVector currentData(8);
    MHDL_NAMED(currentData);
    /*
    BitVector dataZero(8);
    MHDL_NAMED(dataZero);
    */
    
    auto shiftedData = currentData >> 1;
    MHDL_NAMED(shiftedData);
    
    auto loadingData = idle & send;
    MHDL_NAMED(loadingData);
    
    auto newBitCounter = mux(idle, bitCounter + 1_uvec, 0b0000_uvec);
    MHDL_NAMED(newBitCounter);
    driveWith(bitCounter, reg(newBitCounter, enable, 0b0000_uvec));
    
    auto done = bitCounter[3];
    MHDL_NAMED(done);
    
    auto nextData = mux(loadingData, shiftedData, inputData);
    MHDL_NAMED(nextData);
    
    
    {
        PriorityConditional<Bit> con;
        
        con
            .addCondition(done, 1_bit)           // Send stop bit if done
            .addCondition(!idle, currentData[0]) // Send data bits if not idle
            .addCondition(loadingData, 0_bit);   // Send start bit while loading data
            
        // Default to high (idle state).
        outputLine = con(1_bit);
    }
    

    driveWith(currentData, reg(nextData, enable, 0x00_vec));
    
    auto nextIdle = idle;
    //MHDL_NAMED(nextIdle);
    // if loading new data not idle in next step
    nextIdle = mux(loadingData, nextIdle, 0_bit);
    // if done, idle in next step
    nextIdle = mux(done, nextIdle, 1_bit);
        
    driveWith(idle, reg(nextIdle, enable, 1_bit));
}

