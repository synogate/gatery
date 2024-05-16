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
#include <gatery/scl/stream/Stream.h>
#include <gatery/scl/stream/CreditStream.h>

namespace gtry::scl
{
	/**
	 * @brief Allocate unique ID's, which can be used to identify transactions.
	 * @param free Submit any ID that is free to be reused
	 * @param numIds The total number of ID's that can be allocated. All ID's will be in the range [0, numIds). If not specified, the number of ID's will be the maximum value of the UInt width.
	 */
	RvStream<UInt> idAllocator(VStream<UInt> free, std::optional<size_t> numIds = {});

	/**
	 * @brief Allocate unique ID's in ascending order, which can be used to identify transactions. This is similar to idAllocator but ID's cannot be freed out of order.
	 * @param free Signals that the next ID is free to be reused again.
	 * @param numIds The total number of ID's that can be allocated. All ID's will be in the range [0, numIds).
	*/
	RvStream<UInt> idAllocatorInOrder(Bit free, size_t numIds);
}
