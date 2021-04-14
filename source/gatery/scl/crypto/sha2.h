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
#include <gatery/frontend.h>
#include "HashEngine.h"
#include "../Adder.h"

namespace gtry::scl
{
	template<typename TAdder = CarrySafeAdder>
	struct Sha2_256
	{
		enum {
			NUM_ROUNDS = 64,
			HASH_WIDTH = 256,
			BLOCK_WIDTH = 512
		};

		BOOST_HANA_DEFINE_STRUCT(Sha2_256,
			(BVec, hash),
			(BVec, a),
			(BVec, b),
			(BVec, c),
			(BVec, d),
			(BVec, e),
			(BVec, f),
			(BVec, g),
			(BVec, h),
			(std::array<BVec, 16>, w),
			(std::vector<BVec>, kTable)
		);

		Sha2_256() :
			kTable{ {
				"x428a2f98", "x71374491", "xb5c0fbcf", "xe9b5dba5", "x3956c25b", "x59f111f1", "x923f82a4", "xab1c5ed5",
				"xd807aa98", "x12835b01", "x243185be", "x550c7dc3", "x72be5d74", "x80deb1fe", "x9bdc06a7", "xc19bf174",
				"xe49b69c1", "xefbe4786", "x0fc19dc6", "x240ca1cc", "x2de92c6f", "x4a7484aa", "x5cb0a9dc", "x76f988da",
				"x983e5152", "xa831c66d", "xb00327c8", "xbf597fc7", "xc6e00bf3", "xd5a79147", "x06ca6351", "x14292967",
				"x27b70a85", "x2e1b2138", "x4d2c6dfc", "x53380d13", "x650a7354", "x766a0abb", "x81c2c92e", "x92722c85",
				"xa2bfe8a1", "xa81a664b", "xc24b8b70", "xc76c51a3", "xd192e819", "xd6990624", "xf40e3585", "x106aa070",
				"x19a4c116", "x1e376c08", "x2748774c", "x34b0bcb5", "x391c0cb3", "x4ed8aa4a", "x5b9cca4f", "x682e6ff3",
				"x748f82ee", "x78a5636f", "x84c87814", "x8cc70208", "x90befffa", "xa4506ceb", "xbef9a3f7", "xc67178f2"
			} }
		{
			a = "x6a09e667";
			b = "xbb67ae85";
			c = "x3c6ef372";
			d = "xa54ff53a";
			e = "x510e527f";
			f = "x9b05688c";
			g = "x1f83d9ab";
			h = "x5be0cd19";

			hash = pack(a, b, c, d, e, f, g, h);
		}

		void beginBlock(const BVec& _block)
		{
			for (size_t i = 0; i < w.size(); ++i)
				w[i] = _block(Selection::Symbol(w.size() - 1 - i, 32));
		}

		void round(const BVec& round)
		{
			// update state
			const BVec s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
			const BVec s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
			const BVec ch = (e & f) ^ (~e & g);
			const BVec maj = (a & b) ^ (a & c) ^ (b & c);
			const BVec k = mux(round, kTable);

			TAdder tmp = TAdder{} + h + w[0] + k + s1 + ch;
			h = g;
			g = f;
			f = e;
			e = tmp + d;
			d = c;
			c = b;
			b = a;
			a = tmp + s0 + maj;

			// extend message
			const BVec ws0 = rotr(w[ 1],  7) ^ rotr(w[ 1], 18) ^ zshr(w[ 1],  3);
			const BVec ws1 = rotr(w[14], 17) ^ rotr(w[14], 19) ^ zshr(w[14], 10);
			TAdder next_w = TAdder{} + w[0] + w[9] + ws0 + ws1;

			for (size_t i = 0; i < 15; ++i)
				w[i] = w[i + 1];
			w[15] = next_w;
		}

		void endBlock()
		{
			a += hash(Selection::Symbol(7, 32));
			b += hash(Selection::Symbol(6, 32));
			c += hash(Selection::Symbol(5, 32));
			d += hash(Selection::Symbol(4, 32));
			e += hash(Selection::Symbol(3, 32));
			f += hash(Selection::Symbol(2, 32));
			g += hash(Selection::Symbol(1, 32));
			h += hash(Selection::Symbol(0, 32));

			hash = pack(a, b, c, d, e, f, g, h);
		}

		const BVec& finalize() { return hash; }
	};
}
