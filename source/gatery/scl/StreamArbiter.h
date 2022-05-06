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
		arbitrateInOrder(Stream<T>& in0, Stream<T>& in1);
	};

	template<typename T>
	inline arbitrateInOrder<T>::arbitrateInOrder(Stream<T>& in0, Stream<T>& in1)
	{
		auto entity = Area{ "arbitrateInOrder" }.enter();

		Stream<T>::ready = Bit{};
		in0.ready = *Stream<T>::ready;
		in1.ready = *Stream<T>::ready;

		// simple fsm state 0 is initial and state 1 is push upper input
		Bit selectionState;
		HCL_NAMED(selectionState);

		Stream<T>::data = in0.data;
		Stream<T>::valid = in0.valid;
		IF(selectionState == '1' | !*in0.valid)
		{
			Stream<T>::data = in1.data;
			Stream<T>::valid = in1.valid;
		}

		IF(*Stream<T>::ready)
		{
			IF(	selectionState == '0' & 
				*in0.valid & *in1.valid)
			{
				selectionState = '1';
			}
			ELSE
			{
				selectionState = '0';
			}

			IF(selectionState == '1')
			{
				in0.ready = '0';
				in1.ready = '0';
			}
		}
		selectionState = reg(selectionState, '0');
	}
}
