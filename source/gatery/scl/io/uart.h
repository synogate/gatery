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
	struct UartConfig
	{
		// the smallest baud generator step is 2^x which reduces complexity and baud rate precision for very small or non-standard baud rates
		size_t baudGeneratorLogStepSize = 7;
	};

	VStream<BVec> uartRx(Bit rx, UInt baudRate, UartConfig cfg = {});
	Bit uartTx(RvStream<BVec> data, UInt baudRate, UartConfig cfg = {});

	inline auto uartTx(UInt baudRate, UartConfig cfg = {})
	{
		return [=](auto&& in) { return uartTx(std::forward<decltype(in)>(in), baudRate, cfg); };
	}

	Bit baudRateGenerator(Bit setToHalf, UInt baudRate, size_t baudGeneratorLogStepSize);

struct UART
{
	unsigned stabilize_rx = 2;

	bool deriveClock = false;
	unsigned startBits = 1;
	unsigned stopBits = 1;
	unsigned dataBits = 8;
	unsigned baudRate = 19200;

	struct Stream {
		UInt data;
		Bit valid;
		Bit ready;
	};

	Stream receive(Bit rx);

	Bit send(Stream &stream);
};

}
