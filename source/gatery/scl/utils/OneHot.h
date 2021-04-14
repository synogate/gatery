/*  This file is part of Gatery, a library for circuit design.
    Copyright (C) 2021 Synogate GbR

    Gatery is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 3 of the License, or (at your option) any later version.

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

#include <gatery/frontend.h>

#include "../Stream.h"

namespace gtry::scl
{
	struct OneHot : BVec
	{
		OneHot() : BVec() {}
		OneHot(const OneHot&) = default;

		OneHot(BitWidth width) : BVec(width, Expansion::none) {}
		explicit OneHot(const BVec& initValue) : BVec(initValue) {}

		OneHot& operator = (const OneHot&) = default;

		void setBit(const BVec& idx);
	};

	OneHot decoder(const BVec& in);
	BVec encoder(const OneHot& in);

	std::vector<Stream<BVec>> makeIndexList(const BVec& valids);

	template<typename T, typename Iter>
	Stream<T> priorityEncoder(Iter begin, Iter end);

	struct EncoderResult
	{
		BVec index;
		Bit valid;
	};

	EncoderResult priorityEncoder(const BVec& in);
	EncoderResult priorityEncoderTree(const BVec& in, bool registerStep, size_t resultBitsPerStep = 2);


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
		ret.value() = gtry::ConstBVec(maxWidth);

		Bit anyValid = '0';
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

BOOST_HANA_ADAPT_STRUCT(gtry::scl::EncoderResult, index, valid);
