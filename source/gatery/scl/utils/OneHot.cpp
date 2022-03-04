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
#include "OneHot.h"

using namespace gtry;

gtry::scl::OneHot gtry::scl::decoder(const UInt& in)
{
    OneHot ret = BitWidth{ 1ull << in.size() };
    ret.setBit(in);
    return ret;
}

UInt gtry::scl::encoder(const OneHot& in)
{
    UInt ret = BitWidth{ utils::Log2C(in.size()) };

    ret = 0;
    for (size_t i = 0; i < in.size(); ++i)
        ret |= ext(i & in[i]);

    return ret;
}

std::vector<gtry::scl::Stream<gtry::UInt>> gtry::scl::makeIndexList(const UInt& valids)
{
    std::vector<Stream<UInt>> ret(valids.size());
    for (size_t i = 0; i < valids.size(); ++i)
    {
        ret[i].value() = i;
        ret[i].valid = valids[i];
    }
    return ret;
}

gtry::scl::EncoderResult gtry::scl::priorityEncoder(const UInt& in)
{
    if (in.empty())
        return { UInt(0_b), '0' };

    UInt ret = ConstUInt(BitWidth::count(in.size()));
    for (size_t i = in.size() - 1; i < in.size(); --i)
        IF(in[i])
            ret = i;

    return { ret, in != 0 };
}

gtry::scl::EncoderResult gtry::scl::priorityEncoderTree(const UInt& in, bool registerStep, size_t bps)
{
    const size_t stepBits = 1ull << bps;
    const size_t inBitsPerStep = utils::nextPow2((in.size() + stepBits - 1) / stepBits);

    if (inBitsPerStep <= 1)
        return gtry::scl::priorityEncoder(in);

    std::vector<EncoderResult> lowerStep;
    for (size_t i = 0; i < in.size(); i += inBitsPerStep)
    {
        const size_t clamp = std::min(inBitsPerStep, in.size() - i);
        lowerStep.push_back(priorityEncoderTree(in(i, clamp), registerStep, bps));
    }
    setName(lowerStep, "lowerStep");

    EncoderResult lowSelect{
        ConstUInt(BitWidth::count(inBitsPerStep)),
        '0'
    };
    setName(lowSelect, "lowSelect");

    UInt highSelect = ConstUInt(BitWidth{ bps });
    HCL_NAMED(highSelect);

    for (size_t i = lowerStep.size() - 1; i < lowerStep.size(); --i)
        IF(lowerStep[i].valid)
        {
            highSelect = i;
            lowSelect.index = zext(lowerStep[i].index);
            lowSelect.valid = '1';
        }

    EncoderResult out{
        pack(highSelect, lowSelect.index),
        lowSelect.valid
    };
    HCL_NAMED(out);

    if (registerStep)
    {
        // TODO: reg(compound)
        out.index = reg(out.index);
        out.valid = reg(out.valid);
    }
    return out;
}

void gtry::scl::OneHot::setBit(const UInt& idx)
{
    // TODO: remove workaround false signal loop
    (UInt&)*this = 0;

    for (size_t i = 0; i < size(); ++i)
        at(i) = idx == i;
}
