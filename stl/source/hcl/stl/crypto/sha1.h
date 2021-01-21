#pragma once
#include <hcl/frontend.h>
#include "HashEngine.h"
#include "../Adder.h"

namespace hcl::stl
{
	template<typename TVec = BVec, typename TAdder = CarrySafeAdder>
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

		Sha1Generator()
		{
			a = "x67452301";
			b = "xEFCDAB89";
			c = "x98BADCFE";
			d = "x10325476";
			e = "xC3D2E1F0";

			hash = pack(a, b, c, d, e);
		}

		void beginBlock(const TVec& _block)
		{
			for (size_t i = 0; i < w.size(); ++i)
				w[i] = _block(Selection::Symbol(w.size() - 1 - i, 32));
		}

		void round(const BVec& round)
		{
			// select round constant
			TVec k = 0xCA62C1D6;

			IF(round < 20)
				k = 0x5A827999;
			ELSE IF(round < 40)
				k = 0x6ED9EBA1;
			ELSE IF(round < 60)
				k = 0x8F1BBCDC;

			// select round function
			TVec f = b ^ c ^ d;
			IF(round < 20)
				f = (b & c) | (~b & d);
			ELSE IF(round >= 40 & round < 60)
				f = (b & c) | (b & d) | (c & d);

			// update state
			TVec tmp = TAdder{} + rotl(a, 5) + e + w[0] + k + f;
			e = d;
			d = c;
			c = rotl(b, 30);
			b = a;
			a = tmp;

			// extend message
			const BVec next_w = rotl(w[13] ^ w[8] ^ w[2] ^ w[0], 1);
			
			for (size_t i = 0; i < 15; ++i)
				w[i] = w[i + 1];
			w[15] = next_w;
		}

		void endBlock()
		{
			a += hash(Selection::Symbol(4, 32));
			b += hash(Selection::Symbol(3, 32));
			c += hash(Selection::Symbol(2, 32));
			d += hash(Selection::Symbol(1, 32));
			e += hash(Selection::Symbol(0, 32));

			hash = pack(a, b, c, d, e);
		}

		const TVec& finalize() { return hash; }
	};

	template<typename TVec = BVec, typename TAdder = CarrySafeAdder>
	struct Sha0Generator : Sha1Generator<TVec, TAdder>
	{
		using base = Sha1Generator<TVec, TAdder>;
		using base::a;
		using base::b;
		using base::c;
		using base::d;
		using base::e;
		using base::w;

		// same as sha1 but without rotation during message extension
		void round(const BVec& round)
		{
			// select round constant
			TVec k = 0xCA62C1D6;

			IF(round < 20)
				k = 0x5A827999;
			ELSE IF(round < 40)
				k = 0x6ED9EBA1;
			ELSE IF(round < 60)
				k = 0x8F1BBCDC;

			// select round function
			TVec f = b ^ c ^ d;
			IF(round < 20)
				f = (b & c) | (~b & d);
			ELSE IF(round >= 40 & round < 60)
				f = (b & c) | (b & d) | (c & d);

			// update state
			TVec tmp = TAdder{} + rotl(a, 5) + e + w[0] + k + f;
			e = d;
			d = c;
			c = rotl(b, 30);
			b = a;
			a = tmp;

			// extend message
			const BVec next_w = w[13] ^ w[8] ^ w[2] ^ w[0];

			for (size_t i = 0; i < 15; ++i)
				w[i] = w[i + 1];
			w[15] = next_w;
		}
	};

	extern template struct Sha1Generator<>;
	extern template struct Sha0Generator<>;
}
