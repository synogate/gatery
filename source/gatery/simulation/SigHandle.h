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

#include "../hlim/NodePort.h"
#include "../utils/Traits.h"
#include "BitVectorState.h"

#include <boost/spirit/home/support/container.hpp>

#include <boost/multiprecision/cpp_int.hpp>

#include <span>

namespace gtry::sim {

class SigHandle {
	public:
		SigHandle(hlim::NodePort output) : m_output(output) { }
		void operator=(const SigHandle &rhs) { this->operator=(rhs.eval()); }


		template<typename T, typename Ten = std::enable_if_t<boost::spirit::traits::is_container<T>::value>>
		void operator=(const T& collection);

		template<EnumType T>
		void operator=(T v) { operator=((std::uint64_t) v); }
		void operator=(std::uint64_t v);
		void operator=(std::int64_t v);
		void operator=(std::string_view v);
		void operator=(char v);
		void operator=(const DefaultBitVectorState &state);
		void operator=(const ExtendedBitVectorState &state);
		template<std::same_as<sim::BigInt> BigInt_> // prevent conversion
		void operator=(const BigInt_ &v) { assign(v); }

		/// Interprete the given array of integers as one concatenated bit string to assign to this signal
		void operator=(std::span<const std::byte> rhs);

		/// Interprete the given array of integers as one concatenated bit string to assign to this signal
		template<typename T> requires (std::is_trivially_copyable_v<T>)
		void operator=(std::span<T> array) { operator=(std::as_bytes(array)); }


		template<EnumType T>
		bool operator==(T v) const { if (!allDefined()) return false; return value() == (std::uint64_t) v; }
		bool operator==(std::uint64_t v) const;
		bool operator==(std::int64_t v) const;
		bool operator==(std::string_view v) const;
		bool operator==(char v) const;

		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		bool operator==(std::span<const std::byte> rhs) const;
		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<typename T>  requires (std::is_trivially_copyable_v<T>)
		bool operator==(std::span<T> array) const { return operator==(std::as_bytes(array)); }
		/// Interprete the given array of integers as one concatenated bit string and return true if all bits in the signal are defined and equal to the given bit string.
		template<typename T, size_t N>  requires (std::is_trivially_copyable_v<T>)
		bool operator==(std::span<T, N> array) const { return operator==(std::as_bytes(array)); }


		operator std::uint64_t () const { return value(); }
		operator std::int64_t () const;
		operator bool () const;
		operator char () const;
		//operator std::string () const;
		operator DefaultBitVectorState () const { return eval(); }
		operator sim::BigInt () const;

		/// Split the signal's state into integers and return their values.
		/// @details The value of undefined bits in the returned bit string is arbitrary.
		template<typename T>
		std::vector<T> toVector() const;

		void invalidate();
		void stopDriving();
		bool allDefined() const;
		std::uint64_t defined() const;
		std::uint64_t value() const;

		DefaultBitVectorState eval() const;

		hlim::NodePort getOutput() const { return m_output; }

		void overrideDrivingRegister();

		size_t getWidth() const;
	protected:
		void assign(const sim::BigInt &v);

		hlim::NodePort m_output;
		bool m_overrideRegister = false;
};


template<typename T, typename Ten>
inline void SigHandle::operator=(const T& collection)
{
	auto it_begin = begin(collection);
	auto it_end = end(collection);

	DefaultBitVectorState state;
	if (it_begin != it_end)
	{
		state.resize((it_end - it_begin) * sizeof(*it_begin) * 8);
		size_t offset = 0;
		for (auto it = it_begin; it != it_end; ++it)
		{
			state.insertNonStraddling(sim::DefaultConfig::VALUE, offset, sizeof(*it) * 8, *it);
			state.insertNonStraddling(sim::DefaultConfig::DEFINED, offset, sizeof(*it) * 8, -1);
			offset += sizeof(*it) * 8;
		}
	}
	*this = state;
}

template<typename T>
std::vector<T> SigHandle::toVector() const
{
	HCL_DESIGNCHECK_HINT(getWidth() % (sizeof(T)*8) == 0, "The signal width is not a multiple of the width of the words into which its state is to be split!");
	size_t numWords = getWidth() / (sizeof(T)*8);

	auto state = eval();
	const T* begin = (const T*) state.data(sim::DefaultConfig::VALUE);
	const T* end = begin + numWords;
	return std::vector<T>{begin, end};
}

}
