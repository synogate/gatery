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

sim::SigHandle sim(const OutputPin &pin)
{
    auto driver = pin.getNode()->getDriver(0);
    HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
    return sim(driver);
}

sim::SigHandle sim(const OutputPins &pins)
{
    auto driver = pins.getNode()->getDriver(0);
    HCL_DESIGNCHECK_HINT(driver.node != nullptr, "Can't read unbound output pin!");
    return sim(driver);
}


}