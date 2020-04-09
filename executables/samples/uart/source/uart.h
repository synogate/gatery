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

#pragma once

#include <metaHDL_core/frontend/BitVector.h>
#include <metaHDL_core/frontend/Bit.h>


/**
 * @todo write docs
 */
class UartTransmitter
{
    public:
        using BitVector = mhdl::core::frontend::BitVector;
        using Bit = mhdl::core::frontend::Bit;

        UartTransmitter(unsigned dataBits, unsigned stopBits, unsigned clockCyclesPerBit);
        
        void operator()(const BitVector &inputData, const Bit &send, Bit &outputLine, Bit &idle);
    protected:
        unsigned m_dataBits;
        unsigned m_stopBits; 
        unsigned m_clockCyclesPerBit;
};

