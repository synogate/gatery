/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

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
#include "GCD.h"

using namespace hcl::core::frontend;


hcl::stl::StreamSource<hcl::stl::BVecPair> hcl::stl::binaryGCDStep1(StreamSink<BVecPair>& in, size_t iterationsPerClock)
{
    const size_t width = in.first.getWidth();
    StreamSource<BVecPair> out(BVec{ width }, BVec{ width });

    Register<BVec> a(BitWidth{ width });
    Register<BVec> b(BitWidth{ width });
    Register<BVec> d(BitWidth{ utils::Log2C(width) });
    Register<Bit> active;
    HCL_NAMED(a);
    HCL_NAMED(b);
    HCL_NAMED(d);
    HCL_NAMED(active);
    active.setReset('0');

    in.ready = !active;

    IF(in.valid & in.ready)
    {
        a = in.first;
        b = in.second;
        d = ConstBVec(0, d.getWidth());
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
                BVec abs = pack('0', (BVec&)a) - pack('0', (BVec&)b);
                a = mux(abs.msb(), { (BVec&)a, (BVec&)b }); // TODO: (BVec&) cast?

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

    return out;
}

hcl::stl::StreamSource<hcl::core::frontend::BVec> hcl::stl::shiftLeft(StreamSink<BVecPair>& in, size_t iterationsPerClock)
{
    Register<BVec> a{ BitWidth{in.first.getWidth()} };
    Register<BVec> b{ BitWidth{in.second.getWidth()} };
    Register<Bit> active;
    HCL_NAMED(a);
    HCL_NAMED(b);
    HCL_NAMED(active);
    active.setReset('0');
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

    StreamSource<BVec> out{ in.first.getWidth() };
    out.valid = active & (b != 0);
    (BVec&)out = a;

    IF(out.valid & out.ready)
        active = false;

    return out;
}

hcl::stl::StreamSource<hcl::core::frontend::BVec> hcl::stl::binaryGCD(StreamSink<BVecPair>& in, size_t iterationsPerClock)
{
    GroupScope entity(GroupScope::GroupType::ENTITY);
    entity
        .setName("gcd")
        .setComment("Compute the greatest common divisor of two integers using binary GCD.");

    StreamSource source = binaryGCDStep1(in, iterationsPerClock);
    StreamSink<BVecPair> step1 = source;
    auto step2 = shiftLeft(step1, iterationsPerClock);
    return step2;
}
