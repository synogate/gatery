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

#include "Stream.h"
#include "Fifo.h"

namespace gtry::scl
{
	template<typename T>
	struct arbitrateInOrder : Stream<T>
	{
		arbitrateInOrder(Stream<T>& in0, Stream<T>& in1, size_t queueDepth);

		Fifo<std::array<Valid<T>, 2>> queue;
	};

	template<typename T>
	inline arbitrateInOrder<T>::arbitrateInOrder(Stream<T>& in0, Stream<T>& in1, size_t queueDepth)
	{
		auto entity = Area{ "arbitrateInOrder" }.enter();

		std::array<Valid<T>, 2> inData = {
			Valid<T>{*in0.valid, in0.value()},
			Valid<T>{*in1.valid, in1.value()}
		};
		HCL_NAMED(inData);

		queue.setup(queueDepth, inData);
		queue.push(inData, (*in0.valid | *in1.valid) & !queue.full());

		Bit selectionSetPop;
		std::array<Valid<T>, 2> selectionSet;
		constructFrom(inData, selectionSet);
		queue.pop(selectionSet, selectionSetPop);
		HCL_NAMED(selectionSet);
		HCL_NAMED(selectionSetPop);

		in0.ready = !queue.full();
		in1.ready = !queue.full();

		// simple fsm state 0 is initial and state 1 is push upper input
		Bit selectionState;
		HCL_NAMED(selectionState);

		Stream<T>::value() = selectionSet[0].value();
		Stream<T>::valid = selectionSet[0].valid;
		IF(selectionState == '1' | !selectionSet[0].valid)
		{
			Stream<T>::value() = selectionSet[1].value();
			Stream<T>::valid = selectionSet[1].valid;
		}
		IF(queue.empty())
			Stream<T>::valid = '0';

		selectionSetPop = '0';
		Stream<T>::ready = Bit{};
		IF(*Stream<T>::ready)
		{
			selectionSetPop = !queue.empty();
			IF(	selectionState == '0' & 
				selectionSet[0].valid & selectionSet[1].valid &
				!queue.empty())
			{
				selectionSetPop = '0';
				selectionState = '1';
			}
			ELSE
			{
				selectionState = '0';
			}
		}
		selectionState = reg(selectionState, '0');
	}
}
