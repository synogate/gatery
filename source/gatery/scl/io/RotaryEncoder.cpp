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
#include "RotaryEncoder.h"

#include "../cdc.h"
#include "../stream/strm.h"

namespace gtry::scl
{
	VStream<Enum<RotaryDirection>> rotaryEncoder(Bit a, Bit b)
	{
		Area ent{ "scl_rotaryEncoder", true };
		HCL_NAMED(a);
		HCL_NAMED(b);
		
		const Clock& clock = ClockScope::getClk();
		a.resetValue('1');
		b.resetValue('1');
		a = synchronize(a, clock, clock, { .inStage = false });
		b = synchronize(b, clock, clock, { .inStage = false });

		Bit ra, rb;
		Bit edgea = (a != reg(a)) | name("edgea");
		Bit edgeb = (b != reg(b)) | name("edgeb");

		Stream out = strm::createVStream(Enum(RotaryDirection::clockwise), '0');
		IF(edgeb & ra)
			out = strm::createVStream(Enum(RotaryDirection::counterclockwise), '1');
		IF(edgea & rb)
			out = strm::createVStream(Enum(RotaryDirection::clockwise), '1');

		ra ^= edgea;
		rb ^= edgeb;

		IF((ra & rb))
		{
			ra = '0';
			rb = '0';
		}

		ra = reg(ra, '0') | name("ra");
		rb = reg(rb, '0') | name("rb");

		HCL_NAMED(out);
		return out;
	}

	VStream<Enum<RotaryDirection>> rotaryEncoderDoubleStep(Bit a, Bit b)
	{
		Area ent{ "scl_rotaryEncoderDoubleStep", true };
		HCL_NAMED(a);
		HCL_NAMED(b);

		Stream singleStep = rotaryEncoder(a, b);
		Stream out = strm::createVStream(Enum(RotaryDirection::clockwise), '0');

		SInt ctr = 3_b;
		IF(valid(singleStep) & *singleStep == RotaryDirection::clockwise)
			ctr += SInt{ 1 };
		IF(valid(singleStep) & *singleStep == RotaryDirection::counterclockwise)
			ctr -= SInt{ 1 };

		IF(ctr == SInt{ 2 })
			out = strm::createVStream(Enum(RotaryDirection::clockwise), '1');
		IF(ctr == SInt{ -2 })
			out = strm::createVStream(Enum(RotaryDirection::counterclockwise), '1');
		IF(valid(out))
			ctr = 0;

		ctr = reg(ctr, SInt{ 0 }) | name("ctr");
		HCL_NAMED(out);
		return out;
	}

	VStream<RotaryInputEvent> rotarySwitchInput(Bit a, Bit b, Bit sw, bool doubleStep)
	{
		Area ent{ "scl_rotarySwitchInput", true };

		VStream<Enum<RotaryDirection>> rotEvent;
		if (!doubleStep)
			rotEvent = rotaryEncoder(a, b);
		else
			rotEvent = rotaryEncoderDoubleStep(a, b);

		Bit swDebounced = debounce(synchronize(sw, ClockScope::getClk(), ClockScope::getClk(), { .inStage = false }), 1ms);
		Stream swEvent = strm::createVStream(!swDebounced, edge(swDebounced));

		return strm::createVStream(RotaryInputEvent{
				.rotor = rotEvent,
				.sw = swEvent,
			}, 
			valid(rotEvent) | valid(swEvent)
		) | name("out");
	}

	VStream<RotaryInputEvent> rotarySwitchInput(std::string_view pinPrefix, bool doubleStep)
	{
		return rotarySwitchInput(
			pinIn().setName(std::string{pinPrefix} + "A"),
			pinIn().setName(std::string{pinPrefix} + "B"),
			pinIn().setName(std::string{pinPrefix} + "SW"),
			doubleStep
		);
	}
}
