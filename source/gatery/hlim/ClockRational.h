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

#ifdef _MSC_VER
# pragma warning(push)
# pragma warning(disable: 4146)
# include <boost/rational.hpp>
# pragma warning(pop)
#else
# include <boost/rational.hpp>
#endif

#include <chrono>

namespace gtry::hlim 
{
	class ClockRational : public boost::rational<std::uint64_t>
	{
	public:
		using boost::rational<std::uint64_t>::rational;
		ClockRational(boost::rational<std::uint64_t>&& in) : 
			boost::rational<std::uint64_t>(std::move(in)) 
		{}

		template<typename Tval, intmax_t nom, intmax_t denom>
		ClockRational(std::chrono::duration<Tval, std::ratio<nom, denom>> duration) :
			boost::rational<std::uint64_t>(duration.count() * nom, denom)
		{}
	};

	ClockRational operator * (const ClockRational& lhs, const ClockRational& rhs);
	ClockRational operator / (const ClockRational& lhs, const ClockRational& rhs);
	ClockRational operator + (const ClockRational& lhs, const ClockRational& rhs);
	ClockRational operator - (const ClockRational& lhs, const ClockRational& rhs);

	bool clockLess(const ClockRational& lhs, const ClockRational& rhs);

	bool clockMore(const ClockRational &lhs, const ClockRational &rhs);
	bool operator<(const ClockRational &lhs, const ClockRational &rhs);
	bool operator>(const ClockRational &lhs, const ClockRational &rhs);
	bool operator<=(const ClockRational& lhs, const ClockRational& rhs);
	bool operator>=(const ClockRational& lhs, const ClockRational& rhs);

	ClockRational operator/(const ClockRational& lhs, std::integral auto rhs) {
		return lhs / ClockRational(rhs, 1);
	}

	ClockRational operator/(std::integral auto lhs, const ClockRational& rhs) {
		return ClockRational(lhs, 1) / rhs;
	}

	ClockRational operator*(const ClockRational &lhs, std::integral auto rhs) {
		return lhs * ClockRational(rhs, 1);
	}

	ClockRational operator*(std::integral auto lhs, const ClockRational& rhs) {
		return ClockRational(lhs, 1) * rhs;
	}

	inline size_t floor(const ClockRational &v) { return v.numerator() / v.denominator(); }
	inline size_t ceil(const ClockRational &v) { return (v.numerator() + v.denominator()-1) / v.denominator(); }

	inline double toDouble(const ClockRational &v) { return (double) v.numerator() / v.denominator(); }
	inline double toNanoseconds(const ClockRational &v) { return v.numerator() * 1e9 / v.denominator(); }

	void formatTime(std::ostream &stream, ClockRational time);
}

namespace gtry
{
	using hlim::ClockRational;
	using namespace std::chrono_literals;

	inline ClockRational operator ""_ghz(unsigned long long val) { return ClockRational(val * 1'000'000'000); }
	inline ClockRational operator ""_mhz(unsigned long long val) { return ClockRational(val * 1'000'000); }
	inline ClockRational operator ""_khz(unsigned long long val) { return ClockRational(val * 1'000); }
	inline ClockRational operator ""_hz(unsigned long long val) { return ClockRational(val); }
}
