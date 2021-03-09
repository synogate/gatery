#include "OneHot.h"

using namespace hcl::core::frontend;

hcl::stl::OneHot hcl::stl::decoder(const BVec& in)
{
    OneHot ret = BitWidth{ 1ull << in.size() };
    ret.setBit(in);
    return ret;
}

BVec hcl::stl::encoder(const OneHot& in)
{
    BVec ret = BitWidth{ utils::Log2C(in.size()) };

    ret = 0;
    for (size_t i = 0; i < in.size(); ++i)
        ret |= zext(i & in[i]);

    return ret;
}

std::vector<hcl::stl::Stream<hcl::core::frontend::BVec>> hcl::stl::makeIndexList(const core::frontend::BVec& valids)
{
    std::vector<Stream<BVec>> ret(valids.size());
    for (size_t i = 0; i < valids.size(); ++i)
    {
        ret[i].value() = i;
        ret[i].valid = valids[i];
    }
    return ret;
}

hcl::stl::EncoderResult hcl::stl::priorityEncoder(const BVec& in)
{
    if (in.empty())
        return { BVec(0_b), '0' };

    BVec ret = ConstBVec(utils::Log2C(in.size()));

    for (size_t i = in.size() - 1; i < in.size(); --i)
        IF(in[i])
            ret = i;

    return { ret, in != 0 };
}

hcl::stl::EncoderResult hcl::stl::priorityEncoderTree(const BVec& in, bool registerStep, size_t bps)
{
    const size_t stepBits = 1ull << bps;
    const size_t inBitsPerStep = utils::nextPow2((in.size() + stepBits - 1) / stepBits);

    if (inBitsPerStep <= 1)
        return hcl::stl::priorityEncoder(in);

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

void hcl::stl::OneHot::setBit(const BVec& idx)
{
    // TODO: remove workaround false signal loop
    (BVec&)*this = 0;

    for (size_t i = 0; i < size(); ++i)
        at(i) = idx == i;
}
