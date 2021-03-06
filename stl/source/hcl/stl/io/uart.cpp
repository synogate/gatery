#include "uart.h"

namespace hcl::stl {

using namespace core::frontend;

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

    size_t bitLength = core::hlim::floor(ClockScope::getClk().getAbsoluteFrequency() / baudRate);
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


}