#include "SimSigHandle.h"

namespace hcl::core::frontend {

sim::SigHandle sim(hlim::NodePort output)
{
    return sim::SigHandle(output);
}

sim::SigHandle sim(const Bit &bit)
{
    return sim(bit.getReadPort());
}

sim::SigHandle sim(const BVec &signal)
{
    return sim(signal.getReadPort());
}

sim::SigHandle sim(const InputPin &pin)
{
    return sim({.node=pin.getNode(), .port=0ull});
}

sim::SigHandle sim(const InputPins &pins)
{
    return sim({.node=pins.getNode(), .port=0ull});
}


}