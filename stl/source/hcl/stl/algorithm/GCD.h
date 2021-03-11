#pragma once
#include <hcl/frontend.h>
#include <tuple>

#include "../Stream.h"

namespace hcl::stl
{
	using BVecPair = std::pair<core::frontend::BVec, core::frontend::BVec>;

	StreamSource<BVecPair> binaryGCDStep1(StreamSink<BVecPair>& in, size_t iterationsPerClock = 1);
	StreamSource<core::frontend::BVec> shiftLeft(StreamSink<BVecPair>& in, size_t iterationsPerClock = 1);

	StreamSource<core::frontend::BVec> binaryGCD(StreamSink<BVecPair>& in, size_t iterationsPerClock = 1);


}
