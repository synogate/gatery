#include "GCD.h"

using namespace hcl::core::frontend;


hcl::stl::StreamSource<hcl::stl::BVecPair> hcl::stl::binaryGCDStep1(StreamSink<BVecPair>& in, size_t iterationsPerClock)
{
    const size_t width = in.first.getWidth();
    StreamSource<BVecPair> out(BVec{ width }, BVec{ width });

    Register<BVec> a{ width, Expansion::none };
    Register<BVec> b{ width, Expansion::none };
    Register<BVec> d{ utils::Log2C(width), Expansion::none };
    Register<Bit> active;
    HCL_NAMED(a);
    HCL_NAMED(b);
    HCL_NAMED(d);
    HCL_NAMED(active);
    active.setReset(false);

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
                BVec abs = cat(0b0_bvec, a) - cat(0b0_bvec, b);
                a = mux(abs.msb(), { (BVec&)a, (BVec&)b }); // TODO: (BVec&) cast?

                HCL_COMMENT << "a - b is always even, it is sufficient to build the 1s complement";
                b = (abs(0, b.getWidth()) ^ abs.msb()) >> 1;
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
    Register<BVec> a{ in.first.getWidth(), Expansion::none };
    Register<BVec> b{ in.second.getWidth(), Expansion::none };
    Register<Bit> active;
    HCL_NAMED(a);
    HCL_NAMED(b);
    HCL_NAMED(active);
    active.setReset(false);
    in.ready = !active;

    IF(in.valid & in.ready)
    {
        a = in.first;
        b = in.second;
        active = true;
    }

    for (size_t i = 0; i < iterationsPerClock; ++i)
    {
        IF(b != 0_bvec)
        {
            a <<= 1;
            b -= 1u;
        }
    }

    StreamSource<BVec> out{ in.first.getWidth() };
    out.valid = active & b != 0_bvec;
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
