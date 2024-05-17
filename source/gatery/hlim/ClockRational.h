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

#include <boost/rational.hpp>

namespace gtry::hlim {
	using ClockRational = boost::rational<std::uint64_t>;

	inline bool clockLess(const ClockRational &lhs, const ClockRational &rhs) {
		return lhs.numerator() * rhs.denominator() < rhs.numerator() * lhs.denominator();
	}

	inline bool clockMore(const ClockRational &lhs, const ClockRational &rhs) {
		return lhs.numerator() * rhs.denominator() > rhs.numerator() * lhs.denominator();
	}

	inline bool operator<(const ClockRational &lhs, const ClockRational &rhs) {
		return lhs.numerator() * rhs.denominator() < rhs.numerator() * lhs.denominator();
	}
	inline bool operator>(const ClockRational &lhs, const ClockRational &rhs) {
		return lhs.numerator() * rhs.denominator() > rhs.numerator() * lhs.denominator();
	}

	inline size_t floor(const ClockRational &v) { return v.numerator() / v.denominator(); }
	inline size_t ceil(const ClockRational &v) { return (v.numerator() + v.denominator()-1) / v.denominator(); }

	inline double toDouble(const ClockRational &v) { return (double) v.numerator() / v.denominator(); }
	inline double toNanoseconds(const ClockRational &v) { return v.numerator() * 1e9 / v.denominator(); }

	inline ClockRational operator*(const ClockRational &lhs, int rhs) {
		return lhs * ClockRational(rhs, 1);
	}
	inline ClockRational operator/(const ClockRational &lhs, int rhs) {
		return lhs / ClockRational(rhs, 1);
	}
	inline ClockRational operator*(const ClockRational &lhs, size_t rhs) {
		return lhs * ClockRational(rhs, 1);
	}
	inline ClockRational operator/(const ClockRational &lhs, size_t rhs) {
		return lhs / ClockRational(rhs, 1);
	}


	inline ClockRational operator/(int lhs, const ClockRational &rhs) {
		return ClockRational(lhs, 1) / rhs;
	}
	inline ClockRational operator/(size_t lhs, const ClockRational &rhs) {
		return ClockRational(lhs, 1) / rhs;
	}

	void formatTime(std::ostream &stream, ClockRational time);
}
