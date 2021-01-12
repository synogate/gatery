#include <hcl/frontend.h>
#include <span>

namespace hcl::stl
{
	template<typename TVec>
	struct Sha1Config
	{
		size_t numRounds = 80;

		virtual TVec F(const BVec& round, const std::span<TVec,5> s)
		{
			TVec ret = s[1] ^ s[2] ^ s[3];

			IF(round < 20)
				ret = (s[1] & s[2]) | (~s[1] & s[3]);
			ELSE IF(round >= 40 & round < 60)
				ret = (s[1] & s[2]) | (s[1] & s[3]) | (s[2] & s[3]);

			return ret;
		}

		virtual TVec K(const BVec& round)
		{
			TVec K;
			IF(round < 20)
				K = 0x5A827999;
			ELSE IF(round < 40)
				K = 0x6ED9EBA1;
			ELSE IF(round < 60)
				K = 0x8F1BBCDC;
			ELSE
				K = 0xCA62C1D6;
			return K;
		}

		virtual void stateRound(const BVec& round, std::span<TVec, 5> s, const TVec& w)
		{
			TVec f = F(round, s);
			TVec k = K(round);

			TVec tmp = rotl(s[0], 5) + f + k + w + s[4];
			s[4] = s[3];
			s[3] = s[2];
			s[2] = rotl(s[1], 30);
			s[1] = s[0];
			s[0] = tmp;
		}

		virtual TVec messageRound(std::span<TVec, 16> msg)
		{
			TVec w = msg[0];
			for (size_t i = 0; i < 15; ++i)
				msg[i] = msg[i + 1];

			const size_t l = 15;
			msg[l] = rotl(msg[l-3] ^ msg[l-8] ^ msg[l-14] ^ w, 1);
			return w;
		}

		virtual TVec hashBlock(const TVec& state, const TVec& message)
		{
			std::array<TVec, 5> s;
			const size_t wordSize = state.size() / s.size();
			for (size_t i = 0; i < s.size(); ++i)
				s[i] = state(Selection::Symbol(i, wordSize));

			std::array<TVec, 16> m;
			for (size_t i = 0; i < m.size(); ++i)
				m[i] = message(Selection::Symbol(i, message.size() / m.size()));

			for (size_t r = 0; r < numRounds; ++r)
			{
				TVec w = messageRound(m);
				stateRound(r, s, w);
			}

			TVec ret_state = state;
			for (size_t i = 0; i < s.size(); ++i)
				ret_state(Selection::Symbol(i, wordSize)) += s[i];

			return ret_state;
		}
		
	};

	template<typename TVec>
	struct ShaState
	{
		TVec hash;
		TVec a, b, c, d, e;
		TVec w[16];

		void init()
		{
			a = "0x67452301";
			b = "0xEFCDAB89";
			c = "0x98BADCFE";
			d = "0x10325476";
			e = "0xC3D2E1F0";

			hash = pack(a, b, c, d, e);
		}

		void round(const BVec& round)
		{
			// select round constant
			TVec k;
			IF(round < 20)
				k = 0x5A827999;
			ELSE IF(round < 40)
				k = 0x6ED9EBA1;
			ELSE IF(round < 60)
				k = 0x8F1BBCDC;
			ELSE
				k = 0xCA62C1D6;

			// select round function
			TVec f = b ^ c ^ d;
			IF(round < 20)
				f = (b & c) | (~b & d);
			ELSE IF(round >= 40 & round < 60)
				f = (b & c) | (b & d) | (c & d);

			// update state
			TVec tmp = rotl(a, 5) + f + k + w[0] + e;
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

		TVec finalize()
		{
			a += hash(Selection::Symbol(0, 32));
			b += hash(Selection::Symbol(1, 32));
			c += hash(Selection::Symbol(2, 32));
			d += hash(Selection::Symbol(3, 32));
			e += hash(Selection::Symbol(4, 32));

			hash = pack(a, b, c, d, e);
			return hash;
		}

	};

	extern template class Sha1Config<hcl::BVec>;
	extern template class ShaState<hcl::BVec>;
}
