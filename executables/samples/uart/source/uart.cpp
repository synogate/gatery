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

#include <metaHDL_core/frontend/Registers.h>
#include <metaHDL_core/frontend/Integers.h>
#include <metaHDL_core/frontend/Scope.h>
#include <metaHDL_core/frontend/SignalArithmeticOp.h>
#include <metaHDL_core/frontend/SignalLogicOp.h>
#include <metaHDL_core/frontend/SignalBitshiftOp.h>
#include <metaHDL_core/frontend/SignalMiscOp.h>

#include <metaHDL_core/utils/Exceptions.h>

using namespace mhdl::core::frontend;


UartTransmitter::UartTransmitter(unsigned dataBits, unsigned stopBits, unsigned clockCyclesPerBit) :
            m_dataBits(dataBits), m_stopBits(stopBits), m_clockCyclesPerBit(clockCyclesPerBit) 
{
}
        
void UartTransmitter::operator()(const BitVector &inputData, const Bit &send, Bit &outputLine, Bit &idle)
{
    Scope scope;
    scope.setName(GET_FUNCTION_NAME);

    // todo: constants!
    Bit enable;

    RegisterFactory reg({}, {});
    
    UnsignedInteger bitCounter(5);
    
    BitVector currentData(8);
    BitVector dataZero(8);
    
    auto shiftedData = currentData >> 1;
    auto loadingData = idle & send;
    
    UnsignedInteger bitCounterOne(5);
    UnsignedInteger bitCounterZero(5);
    //bitCounter.driveWith(reg(bitCounter+bitCounterOne, enable, bitCounterZero, idle));
    
    auto done = bitCounter[4];
    
    auto nextData = mux(loadingData, shiftedData, inputData);

    // todo: constants!
    Bit high;
    Bit low;


    
    
    // Default to high (idle state).
    outputLine = high;    
    // Send start bit while loading data
    outputLine = mux(loadingData, outputLine, low);
    // Send data bits if not idle
    outputLine = mux(idle, currentData[0], outputLine);
    // Send stop bit if done
    outputLine = mux(done, outputLine, high);
    

    //currentData.driveWith(reg.reg(nextData, enable, reset, dataZero));
    driveWith(currentData, reg(nextData, enable, dataZero));
    
    auto nextIdle = idle;
    // if loading new data not idle in next step
    nextIdle = mux(loadingData, low, nextIdle);
    // if done, idle in next step
    nextIdle = mux(done, high, nextIdle);
        
    //idle.driveWith(reg.reg(nextIdle, enable, reset));
}

