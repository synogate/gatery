/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2021 Michael Offel, Andreas Ley

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
#include <gatery/frontend.h>
#include "HashEngine.h"
#include "../Adder.h"

namespace gtry::scl
{
	template<typename TVec = UInt, typename TAdder = CarrySafeAdder>
	struct Sha1Generator
	{
		enum {
			NUM_ROUNDS = 80,
			HASH_WIDTH = 160,
			BLOCK_WIDTH = 512
		};

		BOOST_HANA_DEFINE_STRUCT(Sha1Generator,
			(TVec, hash),
			(TVec, a),
			(TVec, b),
			(TVec, c),
			(TVec, d),
			(TVec, e),
			(std::array<TVec,16>, w)
		);

		void init()
		{
			a = "x67452301";
			b = "xEFCDAB89";
			c = "x98BADCFE";
			d = "x10325476";
			e = "xC3D2E1F0";

			hash = cat(a, b, c, d, e);
		}

		void beginBlock(const TVec& _block)
		{
			for (size_t i = 0; i < w.size(); ++i)
				w[i] = _block(Selection::Symbol(w.size() - 1 - i, 32_b));
		}

		void round(const UInt& round, bool rotateW = true)
		{
			// select round constant
			TVec k = 0xCA62C1D6;

			IF(round < 20) {
                k = 0x5A827999;
            } ELSE IF(round < 40) {
                k = 0x6ED9EBA1;
            } ELSE IF(round < 60) {
                k = 0x8F1BBCDC;
            }

			HCL_NAMED(k);

			// select round function
			TVec f = b ^ c ^ d;
			IF(round < 20)
				f = (b & c) | (~b & d);
			ELSE IF(round >= 40 & round < 60)
				f = (b & c) | (b & d) | (c & d);

			HCL_NAMED(f);

			// update state
			TVec tmp = TAdder{} + rotl(a, 5) + e + w[0] + k + f;
			e = d;
			d = c;
			c = rotl(b, 30);
			b = a;
			a = tmp;

			// extend message
			UInt next_w = w[13] ^ w[8] ^ w[2] ^ w[0];
			if(rotateW) // do not rotate for sha0
				next_w = rotl(next_w, 1);

			for (size_t i = 0; i < 15; ++i)
				w[i] = w[i + 1];
			w[15] = next_w;

			HCL_NAMED(a);
			HCL_NAMED(b);
			HCL_NAMED(c);
			HCL_NAMED(d);
			HCL_NAMED(e);
			HCL_NAMED(w);
		}

		void endBlock()
		{
			a += hash(Selection::Symbol(4, 32_b));
			b += hash(Selection::Symbol(3, 32_b));
			c += hash(Selection::Symbol(2, 32_b));
			d += hash(Selection::Symbol(1, 32_b));
			e += hash(Selection::Symbol(0, 32_b));

			hash = cat(a, b, c, d, e);
			HCL_NAMED(hash);
		}

		const TVec& finalize() { return hash; }
	};


	template<typename TVec = UInt, typename TAdder = CarrySafeAdder>
	struct Sha0Generator : Sha1Generator<TVec, TAdder>
	{
		// same as sha1 but without rotation during message extension
		void round(const UInt& round)
		{
			Sha1Generator<TVec, TAdder>::round(round, false);
		}
	};

	extern template struct Sha1Generator<>;
	extern template struct Sha0Generator<>;
}
