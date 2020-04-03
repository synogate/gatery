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

using namespace mhdl::core::frontend;

UartTransmitter::UartTransmitter() :
    inputData(8), send(1),
    outputLine(1), idle(1)
{
    // todo: constants!
    BitVector enable(1);
    BitVector reset(1);

    RegisterFactory reg({}, {});
    
    UnsignedInteger bitCounter(5);
    
    BitVector currentData(8);
    BitVector dataZero(8);
    
    auto shiftedData = currentData >> 1;
    auto loadingData = idle & send;
    
    UnsignedInteger bitCounterOne(5);
    UnsignedInteger bitCounterZero(5);
    bitCounter.assign(reg.reg(bitCounter+bitCounterOne, enable, bitCounterZero, idle));
    
    auto done = bitCounter[4];
    
    auto nextData = select(loadingData, inputData, shiftedData);

    // todo: constants!
    BitVector high(1);
    BitVector low(1);


    
    
    // Default to high (idle state).
    outputLine = high;    
    // Send start bit while loading data
    outputLine = select(loadingData, low, outputLine);
    // Send data bits if not idle
    outputLine = select(idle, outputLine, currentData[0]);
    // Send stop bit if done
    outputLine = select(done, high, outputLine);
    

    currentData.assign(reg.reg(nextData, enable, reset, dataZero));
    
    auto nextIdle = idle;
    // if loading new data not idle in next step
    nextIdle = select(loadingData, low, nextIdle);
    // if done, idle in next step
    nextIdle = select(done, high, nextIdle);
        
    idle.assign(reg.reg(nextIdle, enable, reset, high));
}

