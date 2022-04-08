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
#include "gatery/pch.h"
#include "GCD.h"

using namespace gtry;


gtry::scl::StreamSource<gtry::scl::UIntPair> gtry::scl::binaryGCDStep1(StreamSink<UIntPair>& in, size_t iterationsPerClock)
{
	const size_t width = in.first.size();
	StreamSource<UIntPair> out(UInt{ width }, UInt{ width });

	UInt a = BitWidth{ width };
	UInt b = BitWidth{ width };
	UInt d = BitWidth{ utils::Log2C(width) };
	Bit active;

	HCL_NAMED(a);
	HCL_NAMED(b);
	HCL_NAMED(d);
	HCL_NAMED(active);

	in.ready = !active;

	IF(in.valid & in.ready)
	{
		a = in.first;
		b = in.second;
		d = ConstUInt(0, d.width());
		active = true;
	}

	for (size_t i = 0; i < iterationsPerClock; ++i)
	{
		IF(a != b)
		{
			const Bit a_odd = a.lsb();
			const Bit b_odd = b.lsb();

			IF(!a_odd)
				a >>= 1;
			IF(!b_odd)
				b >>= 1;

			IF(!a_odd & !b_odd)
				d += 1u;

			IF(a_odd & b_odd)
			{
				UInt abs = zext(a, 1) - zext(b, 1);
				a = mux(abs.msb(), { a, b });

				HCL_COMMENT << "a - b is always even, it is sufficient to build the 1s complement";
				b = (abs(0, b.size()) ^ abs.msb()) >> 1;
			}
		}
		
	}

	out.valid = active & a == b;
	out.first = a;
	out.second = b;

	IF(out.valid & out.ready)
		active = false;

	a = reg(a);
	b = reg(a);
	d = reg(a);
	active = reg(active, '0');

	return out;
}

gtry::scl::StreamSource<gtry::UInt> gtry::scl::shiftLeft(StreamSink<UIntPair>& in, size_t iterationsPerClock)
{
	UInt a = BitWidth{in.first.width()};
	UInt b = BitWidth{in.second.width()};
	Bit active;
	HCL_NAMED(a);
	HCL_NAMED(b);
	HCL_NAMED(active);

	in.ready = !active;

	IF(in.valid & in.ready)
	{
		a = in.first;
		b = in.second;
		active = true;
	}

	for (size_t i = 0; i < iterationsPerClock; ++i)
	{
		IF(b != 0)
		{
			a <<= 1;
			b -= 1u;
		}
	}

	StreamSource<UInt> out{ in.first.width() };
	out.valid = active & (b != 0);
	(UInt&)out = a;

	IF(out.valid & out.ready)
		active = false;


	a = reg(a);
	b = reg(a);
	active = reg(active, '0');

	return out;
}

gtry::scl::StreamSource<gtry::UInt> gtry::scl::binaryGCD(StreamSink<UIntPair>& in, size_t iterationsPerClock)
{
	GroupScope entity(GroupScope::GroupType::ENTITY);
	entity
		.setName("gcd")
		.setComment("Compute the greatest common divisor of two integers using binary GCD.");

	StreamSource source = binaryGCDStep1(in, iterationsPerClock);
	StreamSink<UIntPair> step1 = source;
	auto step2 = shiftLeft(step1, iterationsPerClock);
	return step2;
}

uint64_t gtry::scl::gcd(uint64_t a, uint64_t b)
{
	while (b)
	{
		uint64_t t = b;
		b = a % b;
		a = t;
	}
	return a;
}
