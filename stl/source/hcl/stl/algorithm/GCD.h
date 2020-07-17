#pragma once
#include <hcl/frontend.h>
#include <tuple>

#include "Stream.h"

namespace hcl::stl
{
	using BVecPair = std::pair<core::frontend::BVec, core::frontend::BVec>;

	StreamSource<BVecPair> binaryGCDStep1(core::frontend::RegisterFactory& clk, StreamSink<BVecPair>& in, size_t iterationsPerClock = 1);
	StreamSource<core::frontend::BVec> shiftLeft(core::frontend::RegisterFactory& clk, StreamSink<BVecPair>& in, size_t iterationsPerClock = 1);

	StreamSource<core::frontend::BVec> binaryGCD(core::frontend::RegisterFactory& clk, StreamSink<BVecPair>& in, size_t iterationsPerClock = 1);


}
