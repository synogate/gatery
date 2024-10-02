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
#include "GCD.h"

using namespace gtry;


gtry::scl::RvStream<gtry::scl::UIntPair> gtry::scl::binaryGCDStep1(RvStream<UIntPair>& in, size_t iterationsPerClock)
{
	const BitWidth width = in->first.width();
	RvStream<UIntPair> out{ UIntPair{width, width} };

	UInt a = width;
	UInt b = width;
	UInt d = BitWidth::count(width.bits());
	Bit active;

	HCL_NAMED(a);
	HCL_NAMED(b);
	HCL_NAMED(d);
	HCL_NAMED(active);

	ready(in) = !active;

	IF(transfer(in))
	{
		a = in->first;
		b = in->second;
		d = 0;
		active = '1';
	}

	for (size_t i = 0; i < iterationsPerClock; ++i)
	{
		IF(a != b)
		{
			const Bit& a_odd = a.lsb();
			const Bit& b_odd = b.lsb();

			IF(!a_odd)
				a >>= 1;
			IF(!b_odd)
				b >>= 1;

			IF(!a_odd & !b_odd)
				d += 1u;

			IF(a_odd & b_odd)
			{
				UInt abs = zext(a, +1_b) - zext(b, +1_b);
				a = mux(abs.msb(), { a, b });

				HCL_COMMENT << "a - b is always even, it is sufficient to build the 1s complement";
				b = (abs(0, b.width()) ^ abs.msb()) >> 1;
			}
		}
		
	}

	valid(out) = active & a == b;
	out->first = a;
	out->second = b;

	IF(transfer(out))
		active = '0';

	a = reg(a);
	b = reg(b);
	d = reg(d);
	active = reg(active, '0');

	return out;
}

gtry::scl::RvStream<gtry::UInt> gtry::scl::shiftLeft(RvStream<UIntPair>& in, size_t iterationsPerClock)
{
	UInt a = in->first.width();
	UInt b = in->second.width();
	Bit active;
	HCL_NAMED(a);
	HCL_NAMED(b);
	HCL_NAMED(active);

	ready(in) = !active;

	IF(transfer(in))
	{
		a = in->first;
		b = in->second;
		active = '1';
	}

	for (size_t i = 0; i < iterationsPerClock; ++i)
	{
		IF(b != 0)
		{
			a <<= 1;
			b -= 1u;
		}
	}

	RvStream<UInt> out{
		in->first.width(),
		{Ready{}, Valid{active & (b != 0)}}
	};
	*out = a;

	IF(transfer(out))
		active = '0';


	a = reg(a);
	b = reg(b);
	active = reg(active, '0');

	return out;
}

gtry::scl::RvStream<gtry::UInt> gtry::scl::binaryGCD(RvStream<UIntPair>& in, size_t iterationsPerClock)
{
	auto area = Area{ "scl_gcd" }.enter();

	RvStream<UIntPair> step1 = binaryGCDStep1(in, iterationsPerClock);
	HCL_NAMED(step1);
	auto step2 = shiftLeft(step1, iterationsPerClock);
	HCL_NAMED(step2);
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
