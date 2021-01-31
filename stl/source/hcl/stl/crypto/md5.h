#pragma once
#include <hcl/frontend.h>
#include "HashEngine.h"
#include "../Adder.h"

namespace hcl::stl
{
	template<typename TVec = BVec, typename TAdder = CarrySafeAdder>
	struct Md5Generator
	{
		enum {
			NUM_ROUNDS = 64,
			HASH_WIDTH = 128,
			BLOCK_WIDTH = 512
		};

		BOOST_HANA_DEFINE_STRUCT(Md5Generator,
			(TVec, hash),
			(TVec, a),
			(TVec, b),
			(TVec, c),
			(TVec, d),
			(std::array<TVec, 16>, w),
			(std::array<TVec, NUM_ROUNDS>, constants),
			(std::array<BVec, NUM_ROUNDS>, s)
		);

		Md5Generator() :
			constants({ {
				"xD76AA478", "xE8C7B756", "x242070DB", "xC1BDCEEE",
				"xF57C0FAF", "x4787C62A", "xA8304613", "xFD469501",
				"x698098D8", "x8B44F7AF", "xFFFF5BB1", "x895CD7BE",
				"x6B901122", "xFD987193", "xA679438E", "x49B40821",
				"xF61E2562", "xC040B340", "x265E5A51", "xE9B6C7AA",
				"xD62F105D", "x02441453", "xD8A1E681", "xE7D3FBC8",
				"x21E1CDE6", "xC33707D6", "xF4D50D87", "x455A14ED",
				"xA9E3E905", "xFCEFA3F8", "x676F02D9", "x8D2A4C8A",
				"xFFFA3942", "x8771F681", "x6D9D6122", "xFDE5380C",
				"xA4BEEA44", "x4BDECFA9", "xF6BB4B60", "xBEBFBC70",
				"x289B7EC6", "xEAA127FA", "xD4EF3085", "x04881D05",
				"xD9D4D039", "xE6DB99E5", "x1FA27CF8", "xC4AC5665",
				"xF4292244", "x432AFF97", "xAB9423A7", "xFC93A039",
				"x655B59C3", "x8F0CCC92", "xFFEFF47D", "x85845DD1",
				"x6FA87E4F", "xFE2CE6E0", "xA3014314", "x4E0811A1",
				"xF7537E82", "xBD3AF235", "x2AD7D2BB", "xEB86D391"
			} }),
			s({ {
				"5d7", "5d12", "5d17", "5d22",  "5d7", "5d12", "5d17", "5d22",  "5d7", "5d12", "5d17", "5d22",  "5d7", "5d12", "5d17", "5d22",
				"5d5",  "5d9", "5d14", "5d20",  "5d5",  "5d9", "5d14", "5d20",  "5d5",  "5d9", "5d14", "5d20",  "5d5",  "5d9", "5d14", "5d20",
				"5d4", "5d11", "5d16", "5d23",  "5d4", "5d11", "5d16", "5d23",  "5d4", "5d11", "5d16", "5d23",  "5d4", "5d11", "5d16", "5d23",
				"5d6", "5d10", "5d15", "5d21",  "5d6", "5d10", "5d15", "5d21",  "5d6", "5d10", "5d15", "5d21",  "5d6", "5d10", "5d15", "5d21"
			}})
		{
			a = "x67452301";
			b = "xEFCDAB89";
			c = "x98BADCFE";
			d = "x10325476";

			hash = pack(d, c, b, a);
		}

		void beginBlock(const TVec& _block)
		{
			TVec swappedBlock = swapEndian(_block);
			for (size_t i = 0; i < w.size(); ++i)
				w[i] = swappedBlock(i * 32, 32);
		}

		void round(const BVec& round)
		{
			TVec k = mux(round, constants);
			
			// select round function
			TVec f = c ^ (b | ~d);
			BVec g = zext(round, 4)(0, 4); // TODO: allow without first extend if not needed

			IF(round < 16)
			{
				f = (b & c) | (~b & d);
			}
			ELSE IF(round < 32)
			{
				f = (b & d) | (c & ~d);
				g = g * 5 + 1;
			}
			ELSE IF(round < 48)
			{
				f = b ^ c ^ d;
				g = g * 3 + 5;
			}
			ELSE
			{
				g = g * 7;
			}

			TVec m = mux(g, w);
			// update state
			TVec tmp = b + rotl(TAdder{} + a + k + f + m, mux(round, s));
			a = d;
			d = c;
			c = b;
			b = tmp;
		}

		void endBlock()
		{
			a += hash(Selection::Symbol(0, 32));
			b += hash(Selection::Symbol(1, 32));
			c += hash(Selection::Symbol(2, 32));
			d += hash(Selection::Symbol(3, 32));

			hash = pack(d, c, b, a);
		}

		TVec finalize() { return swapEndian(hash); }
	};
}
