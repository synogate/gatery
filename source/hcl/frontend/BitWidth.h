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

namespace hcl 
{

	struct BitWidth
	{
		auto operator <=> (const BitWidth&) const = default;
		uint64_t value = 0;

		operator unsigned long long() const { return value; }

		BitWidth operator + (const BitWidth& rhs) const { return { value + rhs.value }; }
		BitWidth operator * (size_t rhs) const { return { value * rhs }; }

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

}
