/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Michael Offel, Andreas Ley

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#include "gatery/pch.h"
#include "uart.h"

namespace gtry::scl {

UART::Stream UART::recieve(Bit rx)
{
    GroupScope entity(GroupScope::GroupType::ENTITY);
    entity.setName("uart_recv");
                                                                                            HCL_NAMED(rx);
    for (auto i : utils::Range(stabilize_rx))
        rx = reg(rx, true);
                                                                                            rx.setName("rx_stabilized");

    HCL_DESIGNCHECK_HINT(deriveClock == false, "Not implemented yet!");
    HCL_DESIGNCHECK_HINT(startBits == 1, "Not implemented yet!");
    HCL_DESIGNCHECK_HINT(stopBits == 1, "Not implemented yet!");

    size_t bitLength = hlim::floor(ClockScope::getClk().getAbsoluteFrequency() / baudRate);
    size_t oneHalfBitLength = bitLength * 3 / 2;

    BVec counter = BitWidth(1+utils::Log2C(oneHalfBitLength));
    counter = reg(counter, 0);
                                                                                            HCL_NAMED(counter);

    Bit idle;
    idle = reg(idle, true);
                                                                                            HCL_NAMED(idle);

    Bit dataValid = false;
                                                                                            HCL_NAMED(dataValid);

    BVec data = BitWidth(dataBits);
    data = reg(data);
                                                                                            HCL_NAMED(data);

    BVec bitCounter = BitWidth(utils::Log2C(dataBits));
    bitCounter = reg(bitCounter, 0);
                                                                                            HCL_NAMED(bitCounter);


    HCL_COMMENT << "If idle, wait for start bit";
    IF (idle) {
        HCL_COMMENT << "If counter is non-zero, we are still waiting for the last stop bit.";
        IF (counter == 0) {
            HCL_COMMENT << "Check if there is a falling edge, if so wait for 1.5 to sample the middle of each bit.";
            IF (rx == false) {
                idle = false;
                counter = oneHalfBitLength-1;
            }
        } ELSE {
            counter -= 1;
        }
    } ELSE {
        HCL_COMMENT << "If counter is zero, sample and shift into data reg.";
        IF (counter == 0) {

            HCL_COMMENT << "Shift in data.";
            data >>= 1;
            data.setName("data_shifted");
            data.msb() = rx;
            /*
            data = cat(rx, data(1, 7));
            */

            data.setName("data_inserted");

            Bit done = bitCounter == dataBits-1;
                                                                                            HCL_NAMED(done);

            if ((1 << bitCounter.size()) == dataBits) {
                bitCounter += 1;
            } else {
                IF (done)
                    bitCounter = 0;
                ELSE
                    bitCounter += 1;
            }

            IF (done) {
                dataValid = true;
                idle = true;
            }

            HCL_COMMENT << "Restart counter to wait for one bit, even if done to wait for the stop bit to pass.";
            counter = bitLength-1;
        } ELSE {
            counter -= 1;
        }
    }


    Stream stream;
    stream.data = BitWidth(dataBits);
    stream.data = reg(stream.data);

    Bit streamValidReg;
    streamValidReg = reg(streamValidReg, false);
    HCL_NAMED(streamValidReg);

    IF (!streamValidReg & dataValid) {
        streamValidReg = true;
        stream.data = data;
    }

    stream.valid = streamValidReg;

    IF (stream.ready)
        streamValidReg = false;

    HCL_NAMED(stream);
    return stream;
}

Bit UART::send(Stream &stream)
{
    GroupScope entity(GroupScope::GroupType::ENTITY);
    entity.setName("uart_send");

    HCL_DESIGNCHECK_HINT(deriveClock == false, "Not implemented yet!");
    HCL_DESIGNCHECK_HINT(startBits == 1, "Not implemented yet!");
    HCL_DESIGNCHECK_HINT(stopBits == 1, "Not implemented yet!");

    size_t bitLength = hlim::floor(ClockScope::getClk().getAbsoluteFrequency() / baudRate);

    BVec counter = BitWidth(utils::Log2C(bitLength+2));
    counter = reg(counter, 0);
                                                                                            HCL_NAMED(counter);

    BVec data = BitWidth(dataBits+3);
    data = reg(data, 0);
                                                                                            HCL_NAMED(data);

    stream.ready = false;

    Bit tx = true;

    Bit idle;
    idle = data == 0;
    HCL_NAMED(idle);
    IF (idle) {
        stream.ready = true;
        IF (stream.valid) {
            data = pack("0b11", stream.data, '0');
            counter = bitLength+1;
        }
    } ELSE {
        tx = data.lsb();
        IF (counter == 0) {
            counter = bitLength;
            data >>= 1;
        }
    }
    counter -= 1;

    return tx;
}



}
