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
#include "SpiMaster.h"
#include "../stream/utils.h"

#include <gatery/scl/memoryMap/PackedMemoryMap.h>
#include <gatery/scl/memoryMap/MemoryMapConnectors.h>

gtry::scl::RvStream<gtry::BVec> gtry::scl::SpiMaster::generate(RvStream<BVec>& in)
{
	Area area{ "scl_SpiMaster", true };
	HCL_NAMED(m_in);
	RvStream<Bit> outBit{ m_in };

	// generate msb first bit stream
	*in = swapEndian(*in, 1_b);
	RvStream<Bit> inBit = strm::reduceWidth(move(in), 1_b, !valid(in)) 
		| strm::transform((Bit& (BVec::*)())&BVec::lsb);
	HCL_NAMED(inBit);

	Counter stepCounter{ m_clockDiv };
	// hold clock while rx side is stalled
	IF(!stepCounter.isLast() & (!valid(outBit) | ready(outBit)))
		stepCounter.inc();
	IF(!valid(inBit))
		stepCounter.reset();

	ready(inBit) = '0';
	m_clk = '0';

	m_out = m_outIdle;
	IF(valid(inBit))
		m_out = *inBit;

	valid(outBit) = '0';

	enum class State {
		setup,
		latch
	};
	Reg<Enum<State>> state{ State::setup };
	IF(state.current() == State::setup)
	{
		IF(valid(inBit) & stepCounter.isLast())
		{
			state = State::latch;
			stepCounter.reset();
		}
	}
	IF(state.current() == State::latch)
	{
		m_clk = '1';
		IF(stepCounter.isFirst())
			valid(outBit) = '1';
		IF(stepCounter.isLast())
		{
			ready(inBit) = '1';
			state = State::setup;
			stepCounter.reset();
		}
	}

	RvStream<BVec> outBitVec = transform(move(outBit), [](const Bit& b) {
		BVec r = ConstBVec(1_b); 
		r.lsb() = b; 
		return r; 
	});

	RvStream<BVec> out = strm::extendWidth(move(outBitVec), in->width());
	*out = swapEndian(*out, 1_b);
	HCL_NAMED(out);

	HCL_NAMED(m_clk);
	HCL_NAMED(m_out);
	return out;
}

gtry::scl::SpiMaster& gtry::scl::SpiMaster::pin(std::string clock, std::string miso, std::string mosi)
{
	pinOut(m_clk).setName(clock);
	pinOut(m_out).setName(mosi);
	m_in = pinIn().setName(miso);

	return *this;
}

gtry::scl::SpiMaster& gtry::scl::SpiMaster::pinTestLoop()
{
	pin("spi_");
	m_in = m_out;
	return *this;
}

gtry::scl::SpiMaster& gtry::scl::SpiMaster::clockDiv(UInt value)
{
	m_clockDiv = value;
	return *this;
}

gtry::scl::SpiMaster& gtry::scl::SpiMaster::outIdle(bool value)
{
	m_outIdle = value;
	return *this;
}

gtry::scl::SpiMaster& gtry::scl::SpiMaster::simulateDevice(std::shared_ptr<SpiDeviceModel> model, std::optional<Bit> csn)
{
	DesignScope::get()->getCircuit().addSimulationProcess([
		in = m_in,
		out = m_out,
		clk = m_clk,
		csn,
		model
	]() -> SimProcess {
		simu(in) = '1';

		sim::SensitivityList clkevent;
		clkevent.add(clk.readPort());

		uint8_t receiveData = 0;
		uint8_t receiveCnt = 0;
		uint8_t sendData = 0xFF;

		while (true)
		{
			co_await sim::WaitChange(clkevent);

			if (!csn || simu(*csn) == '0')
			{
				receiveData <<= 1;
				if (simu(out) == '1')
					receiveData |= 1;

				if (++receiveCnt == 8)
				{
					//sim::SimulationContext* ctx = sim::SimulationContext::current();
					//ctx->onDebugMessage(nullptr, "SPI: " + std::to_string(receiveData));

					receiveCnt = 0;
					sendData = 0xFF;
					if (model)
						sendData = model->data(receiveData);
				}
			}

			co_await sim::WaitChange(clkevent);

			if (!csn || simu(*csn) == '0')
			{
				simu(in) = sendData & 0x80 ? '1' : '0';
				sendData <<= 1;
			}
		}
	});

	return *this;
}

void gtry::scl::spiMasterDevice(MemoryMap& mmap, std::string mmapName, SpiMasterConfig cfg)
{
	Area ent{ "scl_SPIMaster", true };

	UInt speedDivider = 8_b;
	UInt chipSelect = cfg.chipSelectW;

	RvStream<BVec> spiPut{ 8_b };
	HCL_NAMED(spiPut);

	Bit csn = !chipSelect.lsb();
	pinOut(csn, cfg.pinPrefix + cfg.pinCsn);

	RvStream<BVec> spiGet = strm::fifo(SpiMaster{}
		.pin(cfg.pinPrefix + cfg.pinClk, cfg.pinPrefix + cfg.pinMiSo, cfg.pinPrefix + cfg.pinMoSi)
		.clockDiv(speedDivider)
		.simulateDevice(cfg.simulationModel, csn)
		.generate(spiPut),
		cfg.rxFifoDepth);
	HCL_NAMED(spiGet);

	for (const std::string& pinName : cfg.pinGrounded)
		pinOut(Bit{ '0' }, cfg.pinPrefix + pinName);

	mmap.enterScope(mmapName);
	mapIn(mmap, spiPut, "put");
	mapOut(mmap, spiGet, "get");
	mapIn(mmap, speedDivider, "speed");
	mapIn(mmap, chipSelect, "cs");
	mmap.leaveScope();
}
