#pragma once
#include <span>

#include <hcl/frontend.h>

#include "../algorithm/Stream.h"

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

	std::vector<Stream<core::frontend::BVec>> makeIndexList(const core::frontend::BVec& valids);

	template<typename T, typename Iter>
	Stream<T> priorityEncoder(Iter begin, Iter end);

	struct EncoderResult
	{
		core::frontend::BVec index;
		core::frontend::Bit valid;
	};

	EncoderResult priorityEncoder(const core::frontend::BVec& in);
	EncoderResult priorityEncoderTree(const core::frontend::BVec& in, bool registerStep, size_t resultBitsPerStep = 2);


	// implementation 

	template<typename T, typename Iter>
	Stream<T> priorityEncoder(Iter begin, Iter end)
	{
		Stream<T> ret;
		ret.valid = '0';

		size_t maxWidth = 0;
		for (Iter it = begin; it != end; ++it)
			if (maxWidth < it->value.size())
				maxWidth = it->value.size();
		ret.value = core::frontend::BitWidth{ maxWidth };

		core::frontend::Bit anyValid = '0';
		for(Iter it = begin; it != end; ++it)
		{
			IF(it->valid & !anyValid)
			{
				anyValid = '1';
				ret.value = it->value;
				ret.valid = it->valid;
				it->ready = *ret.ready;
			}
		}
		return ret;
	}

}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::EncoderResult, index, valid);
