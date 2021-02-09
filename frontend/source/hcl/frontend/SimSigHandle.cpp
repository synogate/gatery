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

}