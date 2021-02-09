#pragma once

#include "Signal.h"
#include "Bit.h"
#include "BitVector.h"

#include <hcl/simulation/SigHandle.h>


namespace hcl::core::frontend {

sim::SigHandle sim(hlim::NodePort output);
sim::SigHandle sim(const Bit &bit);
sim::SigHandle sim(const BVec &signal);

}