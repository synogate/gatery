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
#include "GpioPhy.h"

#include <gatery/scl/io/codingNRZI.h>
#include <gatery/scl/stream/utils.h>
#include <gatery/scl/stream/Packet.h>
#include <gatery/scl/flag.h>
#include <gatery/scl/Counter.h>

gtry::scl::usb::GpioPhy::GpioPhy() :
	m_clock{ClockScope::getClk()}
{}

static gtry::scl::RvPacketStream<gtry::Bit> bitStuff(gtry::scl::RvPacketStream<gtry::Bit>& source, size_t repeats)
{
	using namespace gtry;
	using namespace gtry::scl;

	auto scope = Area{ "bitStuff" }.enter();

	Counter c{ repeats + 1 };

	RvPacketStream<Bit> out;
	IF(transfer(out))
	{
		IF(*source)
			c.inc();
		ELSE
			c.reset();
	}

	out <<= source;
	IF(c.isLast())
	{
		ready(source) = '0';
		valid(out) = '1';
		eop(out) = '0';
		*out = '0';
	}
	HCL_NAMED(out);
	return out;
}

static void nrzi(gtry::scl::RvPacketStream<gtry::Bit>& source)
{
	using namespace gtry;
	using namespace gtry::scl;

	auto scope = Area{ "nrzi" }.enter();

	Bit last;
	Bit out = last ^ !*source;
	HCL_NAMED(out);

	IF(transfer(source))
	{
		last = out;
		IF(eop(source))
			last = '1';
	}
	last = reg(last, '1');
	HCL_NAMED(last);

	*source = out;
}

gtry::Bit gtry::scl::usb::GpioPhy::setup(OpMode mode)
{
	auto scope = Area{ "scl_UsbGpioPhy" }.enter();
	HCL_DESIGNCHECK_HINT(mode == OpMode::FullSpeedFunction, "no impl");

	m_clock = ClockScope::getClk();

	Bit dEn, dpOut, dnOut, dpIn, dnIn;

	Clock usbPinClock({.absoluteFrequency = hlim::ClockRational{12'000'000}, .name = "usbclk" });
	{
		Bit dpOut_cdc = allowClockDomainCrossing(dpOut, m_clock, usbPinClock);
		Bit dnOut_cdc = allowClockDomainCrossing(dnOut, m_clock, usbPinClock);
		Bit dEn_cdc = allowClockDomainCrossing(dEn, m_clock, usbPinClock);

		ClockScope scope(usbPinClock);
		std::tie(dpIn, dnIn) = pin({ dpOut_cdc, dnOut_cdc }, dEn_cdc);
		HCL_NAMED(dpIn);
		HCL_NAMED(dnIn);
	}

	VStream<Bit, SingleEnded> lineIn = recoverDataDifferential(
		usbPinClock,
		dpIn, dnIn
	);
	HCL_NAMED(lineIn);

	IF(dEn)
		valid(lineIn) = '0';

	IF(valid(lineIn)) {
		m_status.lineState.lsb() = *lineIn & !lineIn.template get<scl::SingleEnded>().zero;
		m_status.lineState.msb() = !*lineIn & !lineIn.template get<scl::SingleEnded>().zero;
	}
	m_status.lineState = reg(m_status.lineState);
	m_status.sessEnd = '0';
	m_status.sessValid = '0';
	m_status.vbusValid = '1';
	m_status.rxError = '0';
	m_status.hostDisconnect = '0';
	m_status.id = '0';
	m_status.altInt = '0';

	VStream<Bit, SingleEnded> lineInDecoded = decodeNRZI(lineIn, 6);
	HCL_NAMED(lineInDecoded);


	m_status.rxActive = flagInstantSet(valid(lineInDecoded), valid(lineInDecoded) & lineInDecoded.template get<SingleEnded>().zero, '0');
	HCL_NAMED(m_status);

	m_se0 = lineInDecoded.template get<SingleEnded>().zero;
	HCL_NAMED(m_se0);

	generateCrc();
	generateRx(lineInDecoded);
	generateTx(dEn, dpOut, dnOut);

	return '1';
}

gtry::Clock& gtry::scl::usb::GpioPhy::clock()
{
	return m_clock;
}

const gtry::scl::usb::PhyRxStatus& gtry::scl::usb::GpioPhy::status() const
{
	return m_status;
}

gtry::scl::usb::PhyTxStream& gtry::scl::usb::GpioPhy::tx()
{
	return m_tx;
}

gtry::scl::usb::PhyRxStream& gtry::scl::usb::GpioPhy::rx()
{
	return m_rx;
}

gtry::SimProcess gtry::scl::usb::GpioPhy::deviceReset()
{
	lineState(SE0);
	co_await WaitFor({ 512, 12'000'000 });
	lineState(J);
	co_await WaitFor({   2, 12'000'000 });
}

gtry::SimProcess gtry::scl::usb::GpioPhy::send(std::span<const std::byte> data)
{
	return send(data, { 1, 12'000'000 });
}

gtry::SimFunction<std::vector<std::byte>> gtry::scl::usb::GpioPhy::receive(size_t timeoutCycles)
{
	hlim::ClockRational baudRate{ 1, 12'000'000 };

	// wait for preamble
	size_t timeout = 0;
	while(lineState() != K)
	{
		if (timeout++ == timeoutCycles) {
			sim::SimulationContext::current()->onWarning(nullptr, "client response timed out.");
			co_return std::vector<std::byte>{};
		}

		co_await WaitFor(baudRate);
	}

	std::vector<std::byte> data;
	uint8_t dataByte = 0;
	uint8_t bitCounter = 0;
	size_t stuffBitCounter = 0;
	Symbol last = J;
	do
	{
		Symbol current = lineState();
		uint8_t bit = current == last ? 1 : 0;

		dataByte |= bit << bitCounter;
		if (++bitCounter == 8)
		{
			data.push_back(std::byte(dataByte));
			dataByte = 0;
			bitCounter = 0;
		}

		if (current == last && ++stuffBitCounter == 6)
		{
			co_await WaitFor(baudRate);
			current = lineState();
			HCL_ASSERT_HINT(current != last, "stuff error");
		}
		if(current != last)
			stuffBitCounter = 0;

		last = current;
		co_await WaitFor(baudRate);

	} while (lineState() != SE0);

	HCL_ASSERT_HINT(bitCounter == 0, "incomplete byte");
	HCL_ASSERT_HINT(!data.empty() && data[0] == std::byte(0x80), "preamble missing");

	while (lineState() != J)
		co_await WaitFor(baudRate);
	co_await WaitFor(baudRate);

	if (!data.empty())
		data.erase(data.begin());
	co_return data;
}

gtry::SimProcess gtry::scl::usb::GpioPhy::send(std::span<const std::byte> packet, hlim::ClockRational baudRate)
{
	size_t bitStuffCounter = 0;
	co_await send(0x80, bitStuffCounter);

	for (std::byte b : packet)
		co_await send(static_cast<uint8_t>(b), bitStuffCounter, baudRate);

	lineState(SE0);
	co_await WaitFor(baudRate);
	co_await WaitFor(baudRate);
	lineState(J);
	co_await WaitFor(baudRate);
	co_await WaitFor(baudRate);
}

gtry::SimProcess gtry::scl::usb::GpioPhy::send(uint8_t byte, size_t& bitStuffCounter, hlim::ClockRational baudRate)
{
	Symbol state = lineState();
	auto swapLine = [&]() {
		if(state == Symbol::J)			state = Symbol::K;
		else if (state == Symbol::K)	state = Symbol::J;
		lineState(state);

		bitStuffCounter = 0;
	};

	for (size_t i = 0; i < 8; ++i)
	{
		if ((byte & 1) == 0)
			swapLine();
		else
			bitStuffCounter++;

		byte >>= 1;
		co_await WaitFor(baudRate);

		if (bitStuffCounter == 6)
		{
			swapLine();
			co_await WaitFor(baudRate);
		}
	}
}

gtry::scl::usb::GpioPhy::Symbol gtry::scl::usb::GpioPhy::lineState() const
{
	if (simu(get<0>(*m_pins)) == '1' && simu(get<1>(*m_pins)) == '0')
		return J;
	if (simu(get<0>(*m_pins)) == '0' && simu(get<1>(*m_pins)) == '1')
		return K;
	if (simu(get<0>(*m_pins)) == '0' && simu(get<1>(*m_pins)) == '0')
		return SE0;
	return undefined;
}

void gtry::scl::usb::GpioPhy::lineState(Symbol state)
{
	switch (state)
	{
	case J:
		simu(get<0>(*m_pins)) = '1';
		simu(get<1>(*m_pins)) = '0';
		break;
	case K:
		simu(get<0>(*m_pins)) = '0';
		simu(get<1>(*m_pins)) = '1';
		break;
	case SE0:
		simu(get<0>(*m_pins)) = '0';
		simu(get<1>(*m_pins)) = '0';
		break;
	default:
		break;
	}
}

static gtry::Bit pulseExtender(gtry::Bit input, size_t cycles, gtry::Bit reset = '0') {
	using namespace gtry;
	using namespace gtry::scl;
	Area area{ "scl_pulseExtender", true };

	HCL_DESIGNCHECK(cycles != 0);

	Counter pulseCtr(cycles + 1);

	IF(input)
		pulseCtr.reset();

	HCL_NAMED(input);
	Bit ret = flagInstantSet(input, (pulseCtr.isLast() & !input) | reset); HCL_NAMED(ret);

	return ret;
}

void gtry::scl::usb::GpioPhy::generateTx(Bit& en, Bit& p, Bit& n)
{
	HCL_NAMED(m_tx);
	//tap(m_tx);
	RvStream<UInt> txStream{ m_tx.data };
	valid(txStream) = m_tx.valid;
	m_tx.ready = ready(txStream);

	auto txPacketStream = addEopDeferred(txStream, edgeFalling(valid(txStream)));

#if 0 // use this to inject random tx errors
	IF(transfer(txPacketStream))
		IF(Counter{45}.isLast()) 
			*txPacketStream ^= Counter{ 8_b }.value();
#endif

	HCL_NAMED(txPacketStream);
	auto txPreambledStream = strm::insertBeat(move(txPacketStream), 0, 0x80u);
	HCL_NAMED(txPreambledStream);
	auto txBitVecStream = strm::reduceWidth(move(txPreambledStream), 1_b);
	HCL_NAMED(txBitVecStream);
	auto txBitStream = generateTxCrcAppend(txBitVecStream.transform([](const UInt& in) { return in.lsb(); }));
	HCL_NAMED(txBitStream);
	auto txStuffedStream = bitStuff(txBitStream, 6);
	nrzi(txStuffedStream);
	HCL_NAMED(txStuffedStream);

	Counter txTimer{ hlim::floor(m_clock.absoluteFrequency() / hlim::ClockRational{ 12'000'000 }) };
	IF(valid(txStuffedStream))
		txTimer.inc();
	ELSE
		txTimer.reset();

	Bit wait = pulseExtender(
		m_se0,
		hlim::floor(m_clock.absoluteFrequency() / hlim::ClockRational{ 12'000'000 }) * 3);

	txStuffedStream = scl::strm::stall(move(txStuffedStream), wait);
	ready(txStuffedStream) = txTimer.isLast();

	en = valid(txStuffedStream);
	p = *txStuffedStream;
	n = !p;

	Bit se0 = reg(pulseExtender(
		transfer(txStuffedStream) & eop(txStuffedStream),
		hlim::floor(m_clock.absoluteFrequency() / hlim::ClockRational{ 12'000'000 }) * 2), '0');
	HCL_NAMED(se0);

	IF(se0)
	{
		en = '1';
		p = '0';
		n = '0';
	}
}

void gtry::scl::usb::GpioPhy::generateRx(const VStream<Bit, SingleEnded>& in)
{
	VStream<UInt, SingleEnded> inBit = in.transform([](const Bit& in)->UInt{
		return zext(in, 1_b);
	});

	// find end of preamble
	{
		enum State {
			idle, waitForLock, preambleFirst, preambleSecond, data
		};
		Reg<Enum<State>> state{ idle };
		state.setName("preamble_detection_state");
		
		IF(state.current() == idle)
		{
			IF(transfer(in) & *in == '0')
				state = waitForLock;
		}
		
		IF(state.current() == waitForLock)
		{
			IF(transfer(in))
				IF(Counter{ 2 }.isLast())
					state = preambleFirst;
		}
		IF(state.current() == preambleFirst)
		{
			IF(transfer(in) & *in == '0')
				state = preambleSecond;
			IF(transfer(in) & *in != '0')
				state = idle;
		}
		IF(state.current() == preambleSecond)
		{
			IF(transfer(in) & *in == '1')
				state = data;
		}

		Bit se0 = in.template get<SingleEnded>().zero;
		HCL_NAMED(se0);

		IF(transfer(in) & se0)
			state = idle;

		IF(state.current() != data | (transfer(in) & se0))
			valid(inBit) = '0';
	}
	setName(inBit, "in_bit_masked");

	VStream<UInt, SingleEnded> lineInWord = strm::extendWidth(move(inBit), 8_b, !m_status.rxActive);
	HCL_NAMED(lineInWord);

	Bit rxDataActive = flag(valid(lineInWord), !m_status.rxActive);
	HCL_NAMED(rxDataActive);
	m_rx.valid = valid(lineInWord);
	m_rx.sop = !rxDataActive;
	m_rx.data = *lineInWord;

	m_rx.eop = edgeFalling(m_status.rxActive) & rxDataActive;
	Bit requireCrcCheck = flag(m_rx.valid & m_rx.sop & m_rx.data.lower(2_b) != "b10", m_rx.eop);
	HCL_NAMED(requireCrcCheck);
	m_rx.error = m_rx.eop & (!m_crcMatch & requireCrcCheck);
	HCL_NAMED(m_rx);
	//tap(m_rx);

	IF(m_status.rxActive)
	{
		IF(m_rx.valid & m_rx.sop)
		{
			m_crcMode = CombinedBitCrc::Mode::crc5;
			IF(m_rx.data[1])
				m_crcMode = CombinedBitCrc::Mode::crc16;
		}
		Bit firstBitAfterPid;
		firstBitAfterPid = flag(m_rx.valid & m_rx.sop, firstBitAfterPid & transfer(inBit));
		m_crcReset = firstBitAfterPid;
		m_crcIn = inBit->lsb();
		m_crcEn = transfer(inBit);
	}
}

void gtry::scl::usb::GpioPhy::generateCrc()
{
	m_crcMode = reg(m_crcMode);
	HCL_NAMED(m_crcMode);
	HCL_NAMED(m_crcEn);
	HCL_NAMED(m_crcIn);
	HCL_NAMED(m_crcOut);
	HCL_NAMED(m_crcMatch);
	HCL_NAMED(m_crcReset);
	HCL_NAMED(m_crcShiftOut);

	ENIF(m_crcEn)
	{
		CombinedBitCrc crc{ m_crcIn, m_crcMode, m_crcReset, m_crcShiftOut };
		m_crcOut = crc.out();
		m_crcMatch = reg(crc.match());
	}
	m_crcEn = '0';
	m_crcReset = '0';
	m_crcIn = 'X';
	m_crcShiftOut = '0';
}

gtry::scl::RvPacketStream<gtry::Bit> gtry::scl::usb::GpioPhy::generateTxCrcAppend(RvPacketStream<Bit> in)
{
	Area scope{ "generateTxCrcAppend", true };
	HCL_NAMED(in);

	enum State { prefix, data, crc };
	Reg<Enum<State>> state{ prefix };
	state.setName("state");

	Counter bitCounter{ 16 };

	Bit appendCrc;
	appendCrc = reg(appendCrc);
	HCL_NAMED(appendCrc);

	RvPacketStream<Bit> out;
	IF(transfer(out))
		bitCounter.inc();
	out <<= in;

	IF(state.current() == prefix)
	{
		IF(bitCounter.value() == 8 + 0)
			appendCrc = *in;

		IF(valid(in))
			m_crcMode = CombinedBitCrc::Mode::crc16;

		eop(out) &= !appendCrc;
		IF(transfer(in) & appendCrc & bitCounter.isLast())
		{
			state = data;
			IF(eop(in))
				state = crc;
		}
	}

	Bit firstDataBit = flag(state.current() == prefix, state.current() != prefix & transfer(out));
	HCL_NAMED(firstDataBit);

	IF(state.current() == data)
	{
		bitCounter.reset();
		m_crcReset = firstDataBit;
		m_crcEn = transfer(in);
		m_crcIn = *in;
		eop(out) = '0';
		IF(transfer(in) & eop(in))
			state = crc;
	}

	IF(state.current() == crc)
	{
		valid(out) = '1';
		*out = m_crcOut;
		m_crcReset = firstDataBit;
		m_crcEn = transfer(out);
		m_crcShiftOut = '1';
		eop(out) = bitCounter.isLast();
		IF(transfer(out) & eop(out))
			state = prefix;
	}

	HCL_NAMED(out);
	return out;
}

std::tuple<gtry::Bit, gtry::Bit> gtry::scl::usb::GpioPhy::pin(std::tuple<Bit, Bit> out, Bit en)
{
	m_pins = std::tuple<TristatePin, TristatePin>{
		tristatePin(get<0>(out), en).setName("USB_DP"),
		tristatePin(get<1>(out), en).setName("USB_DN")
	};

	DesignScope::get()->getCircuit().addSimulationProcess([pins = *m_pins]() -> SimProcess {
		simu(get<0>(pins)) = '1';
		simu(get<1>(pins)) = '0';
		co_return;
	});

	return {
		get<0>(*m_pins),
		get<1>(*m_pins)
	};
}
