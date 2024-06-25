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
#include "gatery/scl_pch.h"
#include "uart.h"

#include "../Adder.h"

namespace gtry::scl {

UART::Stream UART::receive(Bit rx)
{
	GroupScope entity(GroupScope::GroupType::ENTITY, "uart_recv");

	{
		auto visId = dbg::createAreaVisualization(200, 150);

		std::stringstream content;
		content << "<div style='margin: 3px;padding: 3px;'>";
		content << "<h2>UART receiver</h2>";
		content << "<table>";
		content << "<tr><td>Stabilizer length</td>  <td>" << stabilize_rx << "</td></tr>";
		content << "<tr><td>Start bits</td>  <td>" << startBits << "</td></tr>";
		content << "<tr><td>Data bits</td>   <td>" << dataBits << "</td></tr>";
		content << "<tr><td>Baud rate</td>   <td>" << baudRate << "</td></tr>";
		content << "</table>";
		content << "</div>";
		dbg::updateAreaVisualization(visId, content.str());		
	}




																							HCL_NAMED(rx);
	for ([[maybe_unused]] auto i : utils::Range(stabilize_rx)) {
		rx = reg(rx, true);
		attribute(rx, {.allowFusing=false});
	}
	if (stabilize_rx > 0)
																							rx.setName("rx_stabilized");

	HCL_DESIGNCHECK_HINT(deriveClock == false, "Not implemented yet!");
	HCL_DESIGNCHECK_HINT(startBits == 1, "Not implemented yet!");
	//HCL_DESIGNCHECK_HINT(stopBits == 1, "Not implemented yet!");

	size_t bitLength = hlim::floor(ClockScope::getClk().absoluteFrequency() / baudRate);
	size_t oneHalfBitLength = bitLength * 3 / 2;

	UInt counter = BitWidth(1+utils::Log2C(oneHalfBitLength));
	counter = reg(counter, 0);
																							HCL_NAMED(counter);

	Bit idle;
	idle = reg(idle, true);
																							HCL_NAMED(idle);

	Bit dataValid = false;
																							HCL_NAMED(dataValid);

	UInt data = BitWidth(dataBits);
	data = reg(data);
																							HCL_NAMED(data);

	UInt bitCounter = BitWidth(utils::Log2C(dataBits));
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

			if ((1u << bitCounter.size()) == dataBits) {
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
	GroupScope entity(GroupScope::GroupType::ENTITY, "uart_send");

	{
		auto visId = dbg::createAreaVisualization(200, 160);

		std::stringstream content;
		content << "<div style='margin: 3px;padding: 3px;'>";
		content << "<h2>UART transmitter</h2>";
		content << "<table>";
		content << "<tr><td>Start bits</td>  <td>" << startBits << "</td></tr>";
		content << "<tr><td>Data bits</td>   <td>" << dataBits << "</td></tr>";
		content << "<tr><td>Stop bits</td>   <td>" << stopBits << "</td></tr>";
		content << "<tr><td>Baud rate</td>   <td>" << baudRate << "</td></tr>";
		content << "</table>";
		content << "</div>";
		dbg::updateAreaVisualization(visId, content.str());		
	}


	HCL_DESIGNCHECK_HINT(deriveClock == false, "Not implemented yet!");

	size_t bitLength = hlim::floor(ClockScope::getClk().absoluteFrequency() / baudRate);

	UInt counter = BitWidth(utils::Log2C(bitLength+2));
	counter = reg(counter, 0);
																							HCL_NAMED(counter);

	UInt data = BitWidth(uint64_t(dataBits) + startBits + stopBits);
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
			data = oext(cat(stream.data, ConstUInt(0, BitWidth{startBits})));
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

	Bit uartTx(RvStream<BVec> data, UInt baudRate, UartConfig cfg)
	{
		Area entity("scl_uartTx", true);
		HCL_NAMED(data);

		Bit baud = baudRateGenerator('0', baudRate, cfg.baudGeneratorLogStepSize);
		HCL_NAMED(baud);

		// the counter has 4 phases: 6 = wait for in data, 7 = start bit, 8-15 = data bits, 0-1 = stop bit
		// this is done to be able to reduce the mux for selecting a data bit
		// but we need to skip bits 2-6
		Counter bitCounter(16, 4);

		ready(data) = '0';
		IF(valid(data) & baud)
		{
			bitCounter.inc();
			IF(bitCounter.value() == 1)
			{
				ready(data) = '1';
				bitCounter.load(6);
			}
		}

		ready(data) = baud & bitCounter.isLast();

		Bit out = '1';
		IF(bitCounter.value() == 7)
			out = '0'; // start bit
		IF(bitCounter.value().msb())
			out = mux(bitCounter.value().lower(-1_b), *data);

		HCL_NAMED(out);
		return out;
	}

	VStream<BVec> uartRx(Bit rx, UInt baudRate, UartConfig cfg)
	{
		Area entity("scl_uartRx", true);
		HCL_NAMED(rx);

		Bit baudReset;
		Bit baud = baudRateGenerator(baudReset, baudRate, cfg.baudGeneratorLogStepSize);
		HCL_NAMED(baud);

		enum State { wait, start, data, stop };
		Reg<Enum<State>> state{ wait };
		state.setName("state");

		baudReset = '0';
		IF(state.current() == wait)
		{
			IF(edgeFalling(rx))
			{
				baudReset = '1';
				state = start;
			}
		}

		IF(baud & state.current() == start)
		{
			IF(rx == '0')
				state = data;
			ELSE
				state = wait;
		}

		VStream<BVec> out = strm::createVStream<BVec>(8_b, '0');
		IF(baud & state.current() == data)
		{
			Counter bitCounter(8);
			(*out)[bitCounter.value()] = rx;
			IF(bitCounter.isLast())
			{
				state = stop;
				valid(out) = '1';
			}
		}
		out = reg(out);
		HCL_NAMED(out);

		IF(baud & state.current() == stop)
		{
			IF(rx == '1')
				state = wait;
		}

		return out;
	}

	Bit baudRateGenerator(Bit setToHalf, UInt baudRate, size_t baudGeneratorLogStepSize)
	{
		Area entity("scl_baudRateGenerator", true);
		HCL_NAMED(setToHalf);
		HCL_NAMED(baudRate);

		uint64_t cyclesPerSecond = hlim::ceil(ClockScope::getClk().absoluteFrequency()) >> baudGeneratorLogStepSize;
		UInt baudCounter = BitWidth::count(cyclesPerSecond);
		IF(setToHalf)
			baudCounter = cyclesPerSecond / 2;
		baudCounter = reg(baudCounter, 0);
		HCL_NAMED(baudCounter);

		UInt sum = zext(baudCounter, +1_b) + zext(baudRate.upper(-BitWidth{ baudGeneratorLogStepSize }));
		baudCounter = sum.lower(-1_b);

		Bit out = (sum >= cyclesPerSecond) & !setToHalf;
		HCL_NAMED(out);

		IF(out)
			baudCounter -= cyclesPerSecond;
		return out;
	}
}
