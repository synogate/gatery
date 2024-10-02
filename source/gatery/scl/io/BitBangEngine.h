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
#pragma once
#include <gatery/frontend.h>
#include "../stream/strm.h"

namespace gtry::scl
{
	class BitBangEngine
	{
	public:
		struct Io
		{
			Bit out;
			Bit en;
			Bit openDrain;
			Bit in;

			Bit iobufEnable() const { return mux(openDrain, { en, en & !out }); }
			Bit iobufOut() const { return mux(openDrain, { out, Bit{'0'} }); }
			void pin(std::string name, PinNodeParameter param = {});
		};

		enum class Command : uint8_t
		{
			set_byte0 = 0x80,
			get_byte0 = 0x81,
			set_byte1 = 0x82,
			get_byte1 = 0x83,
			
			loopback_enable = 0x84,
			loopback_disable = 0x85,
			set_clock_div = 0x86,
			flush_buffer = 0x87,
			wait_gpio1_high = 0x88,
			wait_gpio1_low = 0x89,
			
			//clockdiv5_disable = 0x8A,
			//clockdiv5_enable = 0x8B,
			threephase_clock_enable = 0x8C,
			threephase_clock_disable = 0x8D,
			toggle_clock_bits = 0x8E,
			toggle_clock_bytes = 0x8F,

			toggle_clock_gpio1_high = 0x94,
			toggle_clock_gpio1_low = 0x95,
			//adaptive_clock_enable = 0x96,
			//adaptive_clock_disable = 0x97,

			toggle_clock_timeout_gpio1_high = 0x9C,
			toggle_clock_timeout_gpio1_low = 0x9D,

			set_open_drain = 0x9E,

			bad_command = 0xAA,
			bad_command_response = 0xFA,
		};

		struct ConfigState {
			Bit clockDelay; // delay clock output by 1/2 period
			Bit captureEdge; // 1 = the opposite edge of the setup edge, 0 = same as setup edge
			Bit msbFirst; // shift out msb first instead of msb
			Bit clockThreePhase; // extend clock low phase by one cycle and setup + capture in between low cycles
			Bit shiftOut; // shift out data
			Bit shiftIn; // shift in data
			Bit dataLoopback;
			Bit idleClockState;
			Bit tmsOutMode; // msb to TMI, shift others to TMS, next byte after 7 bits of data
			Bit stopClockOnLastBit; // stop clocking after last bit
			Bit stopClockOnSignal; // exit clocking mode if io == stopClockSignal
			Bit stopClockSignal;
			UInt targetPinGroup = 1_b; // select pin group for write operations
		};
	public:

		BitBangEngine& ioClk(size_t index) { m_ioClkIndex = index; return *this; }
		BitBangEngine& ioMosi(size_t index) { m_ioMosiIndex = index; return *this; }
		BitBangEngine& ioMiso(size_t index) { m_ioMisoIndex = index; return *this; }
		BitBangEngine& ioTms(size_t index) { m_ioTmsIndex = index; return *this; }
		BitBangEngine& ioStopClock(size_t index) { m_ioStopClockIndex = index; return *this; }

		// note: the circuit is paused by output ready, breaking stream semantics
		RvStream<BVec> generate(RvStream<BVec> command, size_t numIo = 14);
		Io& io(size_t index) { return m_io.at(index); }

		void pin(std::string prefix, PinNodeParameter param = {});

	private:
		Vector<Io> m_io;
		size_t m_ioClkIndex = 0;
		size_t m_ioMosiIndex = 1;
		size_t m_ioMisoIndex = 2;
		size_t m_ioTmsIndex = 3;
		size_t m_ioStopClockIndex = 5;
	};


}

BOOST_HANA_ADAPT_STRUCT(gtry::scl::BitBangEngine::Io, out, en, openDrain, in);
BOOST_HANA_ADAPT_STRUCT(gtry::scl::BitBangEngine::ConfigState, clockDelay, captureEdge, msbFirst, clockThreePhase, shiftOut, shiftIn, dataLoopback, idleClockState, tmsOutMode, stopClockOnLastBit, stopClockOnSignal, stopClockSignal, targetPinGroup);
