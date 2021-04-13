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
#include "hcl/pch.h"
#include "OneHot.h"

using namespace hcl;

hcl::scl::OneHot hcl::scl::decoder(const BVec& in)
{
    OneHot ret = BitWidth{ 1ull << in.size() };
    ret.setBit(in);
    return ret;
}

BVec hcl::scl::encoder(const OneHot& in)
{
    BVec ret = BitWidth{ utils::Log2C(in.size()) };

    ret = 0;
    for (size_t i = 0; i < in.size(); ++i)
        ret |= zext(i & in[i]);

    return ret;
}

std::vector<hcl::scl::Stream<hcl::BVec>> hcl::scl::makeIndexList(const BVec& valids)
{
    std::vector<Stream<BVec>> ret(valids.size());
    for (size_t i = 0; i < valids.size(); ++i)
    {
        ret[i].value() = i;
        ret[i].valid = valids[i];
    }
    return ret;
}

hcl::scl::EncoderResult hcl::scl::priorityEncoder(const BVec& in)
{
    if (in.empty())
        return { BVec(0_b), '0' };

    BVec ret = ConstBVec(utils::Log2C(in.size()));

    for (size_t i = in.size() - 1; i < in.size(); --i)
        IF(in[i])
            ret = i;

    return { ret, in != 0 };
}

hcl::scl::EncoderResult hcl::scl::priorityEncoderTree(const BVec& in, bool registerStep, size_t bps)
{
    const size_t stepBits = 1ull << bps;
    const size_t inBitsPerStep = utils::nextPow2((in.size() + stepBits - 1) / stepBits);

    if (inBitsPerStep <= 1)
        return hcl::scl::priorityEncoder(in);

    std::vector<EncoderResult> lowerStep;
    for (size_t i = 0; i < in.size(); i += inBitsPerStep)
    {
        const size_t clamp = std::min(inBitsPerStep, in.size() - i);
        lowerStep.push_back(priorityEncoderTree(in(i, clamp), registerStep, bps));
    }
    setName(lowerStep, "lowerStep");

    EncoderResult lowSelect{
        ConstBVec(utils::Log2(inBitsPerStep)),
        '0'
    };
    setName(lowSelect, "lowSelect");

    BVec highSelect = ConstBVec(bps);
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

void hcl::scl::OneHot::setBit(const BVec& idx)
{
    // TODO: remove workaround false signal loop
    (BVec&)*this = 0;

    for (size_t i = 0; i < size(); ++i)
        at(i) = idx == i;
}
