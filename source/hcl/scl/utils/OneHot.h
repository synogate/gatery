/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    Gatery is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/
#pragma once
#include <span>

#include <hcl/frontend.h>

#include "../Stream.h"

namespace hcl::stl
{
	struct OneHot : core::frontend::BVec
	{
		OneHot() : BVec() {}
		OneHot(const OneHot&) = default;

		OneHot(core::frontend::BitWidth width) : BVec(width, core::frontend::Expansion::none) {}
		explicit OneHot(const core::frontend::BVec& initValue) : BVec(initValue) {}

		OneHot& operator = (const OneHot&) = default;

		void setBit(const BVec& idx);
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
			if (maxWidth < it->size())
				maxWidth = it->size();
		ret.value() = hcl::core::frontend::ConstBVec(maxWidth);

		core::frontend::Bit anyValid = '0';
		for(Iter it = begin; it != end; ++it)
		{
			it->ready = '0';

			IF(*it->valid & !anyValid)
			{
				anyValid = '1';
				ret.value() = it->value();
				ret.valid = it->valid;
				it->ready = ret.ready;
			}
		}
		return ret;
	}

}

BOOST_HANA_ADAPT_STRUCT(hcl::stl::EncoderResult, index, valid);
