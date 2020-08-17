#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	struct OneHot : core::frontend::BVec
	{
		OneHot() : BVec() {}
		OneHot(const OneHot&) = default;

		OneHot(core::frontend::BitWidth width) : BVec(width, core::frontend::Expansion::none) {}
		explicit OneHot(const core::frontend::BVec& initValue) : BVec(initValue) {}

		OneHot& operator = (const OneHot&) = default;
	};

	OneHot decoder(const core::frontend::BVec& in);
	core::frontend::BVec encoder(const OneHot& in);

	struct EncoderResult
	{
		core::frontend::BVec index;
		core::frontend::Bit valid;
	};

	EncoderResult priorityEncoder(const core::frontend::BVec& in);
	EncoderResult priorityEncoderTree(const core::frontend::BVec& in, bool registerStep, size_t resultBitsPerStep = 2);

}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::EncoderResult, index, valid);
