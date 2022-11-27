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

#include "../utils/BitManipulation.h"
#include <compare>

namespace gtry 
{
/**
 * @addtogroup gtry_frontend
 * @{
 */
	struct BitExtend { uint64_t value; };
	struct BitReduce { uint64_t value; };

	struct BitWidth
	{
		auto operator <=> (const BitWidth&) const = default;
		bool operator >= (const BitWidth&) const = default;
		bool operator <= (const BitWidth&) const = default;
		bool operator > (const BitWidth&) const = default;
		bool operator < (const BitWidth&) const = default;
		bool operator == (const BitWidth&) const = default;
		bool operator != (const BitWidth&) const = default;

		BitWidth& operator += (const BitWidth& rhs) { value += rhs.value; return *this; }
		BitWidth& operator -= (const BitWidth& rhs) { value -= rhs.value; return *this; }
		BitWidth& operator *= (const uint64_t rhs) { value *= rhs; return *this; }
		BitWidth& operator /= (const uint64_t rhs) { value /= rhs; return *this; }

		BitExtend operator +() const { return { value }; }
		BitReduce operator -() const { return { value }; }

		explicit operator bool() const { return value != 0; }

		uint64_t value = 0;

		BitWidth() = default;
		constexpr explicit BitWidth(uint64_t v) : value(v) { }

		constexpr uint64_t count() const { return 1ull << value; }
		constexpr uint64_t last() const { return count() - 1; }
		constexpr uint64_t mask() const { return (value == 64) ? -1 : (1ull << value) - 1; }
		constexpr size_t bytes() const { return value / 8; }
		constexpr uint64_t bits() const { return value; }
		constexpr size_t numBeats(BitWidth beatSize) const { return (value + beatSize.value - 1) / beatSize.value; }

		constexpr BitWidth nextPow2() const { return BitWidth{ utils::nextPow2(value) }; }

		constexpr bool divisibleBy(uint64_t divisor) const { return value % divisor == 0; }
		constexpr bool divisibleBy(BitWidth divisor) const { return value % divisor.value == 0; }

		inline static BitWidth last(uint64_t value) { return BitWidth{ utils::Log2C(value + 1) }; }
		inline static BitWidth count(uint64_t count) { return BitWidth{ count <= 1 ? 0 : utils::Log2C(count) }; }
	};

	inline namespace literals
	{
		constexpr BitWidth operator"" _b(unsigned long long bit) { return BitWidth{ bit }; }
		constexpr BitWidth operator"" _B(unsigned long long byte) { return BitWidth{ byte * 8 }; }
		constexpr BitWidth operator"" _Kb(unsigned long long kilobit) { return BitWidth{ kilobit * 1000 }; }
		constexpr BitWidth operator"" _KB(unsigned long long kilobyte) { return BitWidth{ kilobyte * 1000 * 8 }; }
		constexpr BitWidth operator"" _Kib(unsigned long long kibibit) { return BitWidth{ kibibit * 1024 }; }
		constexpr BitWidth operator"" _KiB(unsigned long long kibibyte) { return BitWidth{ kibibyte * 1024 * 8 }; }
		constexpr BitWidth operator"" _Mb(unsigned long long megabit) { return BitWidth{ megabit * 1000 * 1000 }; }
		constexpr BitWidth operator"" _MB(unsigned long long megabyte) { return BitWidth{ megabyte * 1000 * 1000 * 8 }; }
		constexpr BitWidth operator"" _Mib(unsigned long long mibibit) { return BitWidth{ mibibit * 1024 * 1024 }; }
		constexpr BitWidth operator"" _MiB(unsigned long long mibibyte) { return BitWidth{ mibibyte * 1024 * 1024 * 8 }; }
		constexpr BitWidth operator"" _Gb(unsigned long long gigabit) { return BitWidth{ gigabit * 1000 * 1000 * 1000 }; }
		constexpr BitWidth operator"" _GB(unsigned long long gigabyte) { return BitWidth{ gigabyte * 1000 * 1000 * 1000 * 8 }; }
		constexpr BitWidth operator"" _Gib(unsigned long long gibibit) { return BitWidth{ gibibit * 1024 * 1024 * 1024 }; }
		constexpr BitWidth operator"" _GiB(unsigned long long gibibyte) { return BitWidth{ gibibyte * 1024 * 1024 * 1024 * 8 }; }
	}

	inline BitWidth operator + (BitWidth l, BitWidth r) { return BitWidth{ l.value + r.value }; }
	inline BitWidth operator + (BitWidth l, uint64_t r) { return BitWidth{ l.value + r }; }
	inline BitWidth operator + (uint64_t l, BitWidth r) { return BitWidth{ l + r.value }; }

	inline BitWidth operator - (BitWidth l, BitWidth r) { return BitWidth{ l.value - r.value }; }
	inline BitWidth operator - (BitWidth l, uint64_t r) { return BitWidth{ l.value - r }; }
	inline BitWidth operator - (uint64_t l, BitWidth r) { return BitWidth{ l - r.value }; }

	inline BitWidth operator * (BitWidth l, BitWidth r) { return BitWidth{ l.value * r.value }; }
	inline BitWidth operator * (BitWidth l, uint64_t r) { return BitWidth{ l.value * r }; }
	inline BitWidth operator * (uint64_t l, BitWidth r) { return BitWidth{ l * r.value }; }

	inline uint64_t operator / (BitWidth l, BitWidth r) { HCL_DESIGNCHECK(l.divisibleBy(r)); return l.value / r.value; }
	inline BitWidth operator / (BitWidth l, uint64_t r) { HCL_DESIGNCHECK(l.divisibleBy(r)); return BitWidth{ l.value / r }; }
	inline uint64_t operator / (uint64_t l, BitWidth r) { HCL_DESIGNCHECK(BitWidth{ l }.divisibleBy(r)); return l / r.value; }

	inline std::ostream& operator << (std::ostream& s, BitWidth width) 
	{ 
		uint64_t val = width.value;
		char u1 = 'b';
		const char* u2 = "";

		if (val == 0) {
			// nothing
		} else if (val % 1'000'000 == 0)
		{
			val /= 1'000'000;
			u2 = "M";
		}
		else if (val % (1024 * 1024) == 0)
		{
			val /= 1024 * 1024;
			u2 = "Mi";
		}
		else if (val % 1000 == 0)
		{
			val /= 1000;
			u2 = "K";
		}
		else if (val % 1024 == 0)
		{
			val /= 1024;
			u2 = "Ki";
		}

		s << val << u2 << u1; return s; 
	}

/**@}*/
}
