#pragma once
#include <hcl/frontend.h>

namespace hcl::stl
{
	struct OneHot : core::frontend::BVec
	{
		OneHot() : BVec() {}
		OneHot(const OneHot&) = default;

		explicit OneHot(size_t width) : BVec(width, core::frontend::Expansion::none) {}
		explicit OneHot(const core::frontend::BVec& initValue) : BVec(initValue) {}

		OneHot& operator = (const OneHot&) = default;
	};

	OneHot decoder(const core::frontend::BVec& in);

	core::frontend::BVec encoder(const OneHot& in);
	core::frontend::BVec priorityEncoder(const core::frontend::BVec& in);

}
