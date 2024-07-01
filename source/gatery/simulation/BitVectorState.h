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

#include "../utils/Exceptions.h"
#include "../utils/Preprocessor.h"
#include "../utils/BitManipulation.h"
#include "../utils/Range.h"

#include <boost/multiprecision/cpp_int.hpp>

#include <vector>
#include <array>
#include <cstdint>
#include <string.h>
#include <span>
#include <random>

namespace gtry::sim {
typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<>> BigInt;



struct DefaultConfig
{
	using BaseType = std::uint64_t;
	enum {
		NUM_BITS_PER_BLOCK = sizeof(BaseType)*8
	};
	enum Plane {
		VALUE,
		DEFINED,
		NUM_PLANES
	};
};

struct ExtendedConfig
{
	using BaseType = std::uint64_t;
	enum {
		NUM_BITS_PER_BLOCK = sizeof(BaseType)*8
	};
	enum Plane {
		VALUE,
		DEFINED,
		DONT_CARE,
		HIGH_IMPEDANCE,
		NUM_PLANES
	};
};


template<class Config>
class BitVectorState
{
	public:
		class iterator
		{
		public:
			class proxy
			{
			public:
				proxy(BitVectorState& state, typename Config::Plane plane, size_t offset, size_t size) :
					m_state(state), m_plane(plane), m_offset(offset), m_size(size) {}

				operator typename Config::BaseType() const { return m_state.extract(m_plane, m_offset, m_size); }
				proxy& operator = (typename Config::BaseType value) { m_state.insert(m_plane, m_offset, m_size, value); return *this; }

			private:
				BitVectorState& m_state;
				const typename Config::Plane m_plane;
				const size_t m_offset;
				const size_t m_size;
			};

			iterator(BitVectorState& state, typename Config::Plane plane, size_t offset, size_t end) :
				m_state(state), m_plane(plane), m_offset(offset), m_end(end) {}

			bool operator != (const iterator& r) const { return m_offset != r.m_offset; }
			bool operator == (const iterator& r) const { return m_offset == r.m_offset; }

			iterator& operator ++() { m_offset += stepWidth(); return *this; }
			iterator operator ++(int) { return iterator{m_state, m_plane, m_offset + stepWidth(), m_end}; }

			proxy operator *() { return proxy{ m_state, m_plane, m_offset, stepWidth() }; }

			size_t stepWidth() const { return std::min(sizeof(typename Config::BaseType) * 8, m_end - m_offset); }
			size_t mask() const { return (1ull << stepWidth()) - 1; }

		private:
			BitVectorState& m_state;
			const typename Config::Plane m_plane;
			size_t m_offset;
			const size_t m_end;
		};

		BitVectorState() = default;

		void resize(size_t size);
		inline size_t size() const { return m_size; }
		inline size_t getNumBlocks() const { return m_values[0].size(); }
		void clear();

		bool get(typename Config::Plane plane, size_t idx = 0) const;
		void set(typename Config::Plane plane, size_t idx);
		void set(typename Config::Plane plane, size_t idx, bool bit);
		void clear(typename Config::Plane plane, size_t idx);
		void toggle(typename Config::Plane plane, size_t idx);

		void setRange(typename Config::Plane plane, size_t offset, size_t size, bool bit);
		void setRange(typename Config::Plane plane, size_t offset, size_t size);
		void clearRange(typename Config::Plane plane, size_t offset, size_t size);
		void copyRange(size_t dstOffset, const BitVectorState<Config> &src, size_t srcOffset, size_t size);
		bool compareRange(size_t dstOffset, const BitVectorState<Config> &src, size_t srcOffset, size_t size) const;


		typename Config::BaseType *data(typename Config::Plane plane);
		const typename Config::BaseType *data(typename Config::Plane plane) const;

		std::span<std::byte> asWritableBytes(typename Config::Plane plane);
		std::span<const std::byte> asBytes(typename Config::Plane plane) const;

		BitVectorState<Config> extract(size_t start, size_t size) const;
		void insert(const BitVectorState& state, size_t offset, size_t size = 0);

		typename Config::BaseType extract(typename Config::Plane plane, size_t offset, size_t size) const;
		typename Config::BaseType extractNonStraddling(typename Config::Plane plane, size_t start, size_t size) const;

		typename Config::BaseType head(typename Config::Plane plane) const { return extractNonStraddling(plane, 0, size()); }

		void insert(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value);
		void insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value);

		std::pair<iterator, iterator> range(typename Config::Plane plane, size_t offset, size_t size);

		bool operator == (const BitVectorState& o) const;

		void append(const BitVectorState<Config> &src);
protected:
		size_t m_size = 0;
		std::array<std::vector<typename Config::BaseType>, Config::NUM_PLANES> m_values;
};

template<typename Config>
bool allDefinedNonStraddling(const BitVectorState<Config> &vec, size_t start, size_t size) {
	return !utils::andNot<typename Config::BaseType>(vec.extractNonStraddling(Config::DEFINED, start, size), utils::bitMaskRange<typename Config::BaseType>(0, size));
}

/**
 * @brief Checks if all bits are true
 * 
 * @param vec Vector to check bits in
 * @param plane Which plane to check (e.g. values, defined-ness)
 * @param start The first index of the range to check, defaults to 0.
 * @param size The number of bits to check, automatically clamped to the size of vec. Defaults to check all bits.
 * @returns Wether all bits of the specified plane in the specified range are true. 
 */
template<typename Config>
bool allOne(const BitVectorState<Config> &vec, typename Config::Plane plane, size_t start = 0ull, size_t size = ~0ull) {

	size = std::min(size, vec.size()-start);

	size_t startFullChunk = (start + Config::NUM_BITS_PER_BLOCK-1) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;
	size_t endFullChunk = (start+size) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;

	if (startFullChunk < endFullChunk) {
		for (size_t c = startFullChunk / Config::NUM_BITS_PER_BLOCK; c < endFullChunk / Config::NUM_BITS_PER_BLOCK; c++)
			if (~vec.data(plane)[c]) return false;

		for (size_t i = start; i < startFullChunk; i++)
			if (!vec.get(plane, i)) return false;

		for (size_t i = endFullChunk; i < start+size; i++)
			if (!vec.get(plane, i)) return false;
	} else {
		for (size_t i = start; i < start+size; i++)
			if (!vec.get(plane, i)) return false;
	}

	return true;
}

/**
 * @brief Checks if all bits are false
 * 
 * @param vec Vector to check bits in
 * @param plane Which plane to check (e.g. values, defined-ness)
 * @param start The first index of the range to check, defaults to 0.
 * @param size The number of bits to check, automatically clamped to the size of vec. Defaults to check all bits.
 * @returns Wether all bits of the specified plane in the specified range are false. 
 */
template<typename Config>
bool allZero(const BitVectorState<Config> &vec, typename Config::Plane plane, size_t start = 0ull, size_t size = ~0ull) {

	size = std::min(size, vec.size()-start);

	size_t startFullChunk = (start + Config::NUM_BITS_PER_BLOCK-1) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;
	size_t endFullChunk = (start+size) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;

	if (startFullChunk < endFullChunk) {
		for (size_t c = startFullChunk / Config::NUM_BITS_PER_BLOCK; c < endFullChunk / Config::NUM_BITS_PER_BLOCK; c++)
			if (vec.data(plane)[c]) return false;

		for (size_t i = start; i < startFullChunk; i++)
			if (vec.get(plane, i)) return false;

		for (size_t i = endFullChunk; i < start+size; i++)
			if (vec.get(plane, i)) return false;
	} else {
		for (size_t i = start; i < start+size; i++)
			if (vec.get(plane, i)) return false;
	}

	return true;
}

/**
 * @brief Checks if all bits are defined
 * 
 * @param vec Vector to check bits in
 * @param start The first index of the range to check, defaults to 0.
 * @param size The number of bits to check, automatically clamped to the size of vec. Defaults to check all bits.
 * @returns Wether all bits of the Config::DEFINED plane in the specified range are true. 
 */
template<typename Config>
bool allDefined(const BitVectorState<Config> &vec, size_t start = 0ull, size_t size = ~0ull) {
	return allOne(vec, Config::DEFINED, start, size);
}

/**
 * @brief Checks if any bits are defined
 * 
 * @param vec Vector to check bits in
 * @param start The first index of the range to check, defaults to 0.
 * @param size The number of bits to check, automatically clamped to the size of vec. Defaults to check all bits.
 * @returns Wether any bits of the Config::DEFINED plane in the specified range are true. 
 */
template<typename Config>
bool anyDefined(const BitVectorState<Config> &vec, size_t start = 0ull, size_t size = ~0ull) {
	size = std::min(size, vec.size()-start);

	size_t startFullChunk = (start + Config::NUM_BITS_PER_BLOCK-1) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;
	size_t endFullChunk = (start+size) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;

	if (startFullChunk < endFullChunk) {
		for (size_t c = startFullChunk / Config::NUM_BITS_PER_BLOCK; c < endFullChunk / Config::NUM_BITS_PER_BLOCK; c++)
			if (vec.data(Config::DEFINED)[c]) return true;

		for (size_t i = start; i < startFullChunk; i++)
			if (vec.get(Config::DEFINED, i)) return true;

		for (size_t i = endFullChunk; i < start+size; i++)
			if (vec.get(Config::DEFINED, i)) return true;
	} else {
		for (size_t i = start; i < start+size; i++)
			if (vec.get(Config::DEFINED, i)) return true;
	}

	return false;
}

template<typename Config>
bool compareValues(const BitVectorState<Config> &vecA, size_t startA, const BitVectorState<Config> &vecB, size_t startB, size_t size) {

	/// @todo: optimize
	for (size_t i = 0; i < size; i++)
		if (vecA.get(Config::VALUE, startA+i) != vecB.get(Config::VALUE, startB+i)) return false;

	return true;
}


template<typename Config>
bool equalOnDefinedValues(const BitVectorState<Config> &vecA, size_t startA, const BitVectorState<Config> &vecB, size_t startB, size_t size) {

	/// @todo: optimize

	for (size_t i = 0; i < size; i++) {
		bool aDef = vecA.get(Config::DEFINED, startA+i);
		bool bDef = vecB.get(Config::DEFINED, startB+i);
		if (aDef != bDef) return false;
		if (aDef)
			if (vecA.get(Config::VALUE, startA+i) != vecB.get(Config::VALUE, startB+i)) return false;
	}

	return true;
}


template<typename Config>
bool canBeReplacedWith(const BitVectorState<Config> &vecA, const BitVectorState<Config> &vecB, size_t startA = 0, size_t startB = 0, size_t size = ~0ull) {

	/// @todo: optimize

	if (size == ~0ull)
		size = vecA.size() - startA;

	for (size_t i = 0; i < size; i++) {
		bool aDef = vecA.get(Config::DEFINED, startA+i);
		if (!aDef) continue; // If A is undefined, any value in B is fine

		bool bDef = vecB.get(Config::DEFINED, startB+i);
		if (!bDef) return false; // If A is defined and B is undefined, A can't be replaced by B

		bool aVal = vecA.get(Config::VALUE, startA+i);
		bool bVal = vecB.get(Config::VALUE, startB+i);

		if (aVal != bVal) return false;
	}

	return true;
}

/**
 * @brief Converts an entire defaultBitVector to a boost::multiprecision::number
 * @param vec BitVectorState to extract the bits from
 * @return BigInt boost::multiprecision::number
*/
template<typename Config>
BigInt extractBigInt(const BitVectorState<Config>& vec) {
	return extractBigInt(vec, 0, vec.size());
}

/**
 * @brief Extracts a value range of bits from a BitVectorState and converts it as a boost::multiprecision::number to facilitate big int computations.
 * @param vec BitVectorState to extract the bits from
 * @param offset offset of the range to extract in bits
 * @param size size of the range to extract in bits
 * @return BigInt boost::multiprecision::number of the values in the range
 */
template<typename Config>
BigInt extractBigInt(const BitVectorState<Config> &vec, size_t offset, size_t size)
{
	if (size <= Config::NUM_BITS_PER_BLOCK) {
		return vec.extract(Config::VALUE, offset, size);
	} else {
		HCL_ASSERT(offset % Config::NUM_BITS_PER_BLOCK == 0);

		BigInt partialChunk;

		size_t lastChunkOffset = (offset + size) / Config::NUM_BITS_PER_BLOCK * Config::NUM_BITS_PER_BLOCK;
		size_t lastChunkWidth = size - (lastChunkOffset - offset);
		if (lastChunkWidth > 0)
			partialChunk = vec.extractNonStraddling(Config::VALUE, lastChunkOffset, lastChunkWidth);
		else
			partialChunk = 0;

		BigInt fullChunkPart;
		boost::multiprecision::import_bits(
			fullChunkPart, 
			vec.data(Config::VALUE) + offset / Config::NUM_BITS_PER_BLOCK, 
			vec.data(Config::VALUE) + (offset + size) / Config::NUM_BITS_PER_BLOCK,
			Config::NUM_BITS_PER_BLOCK,
			false
		);

		return fullChunkPart | (partialChunk << (lastChunkOffset - offset));
	}
}
extern template BigInt extractBigInt<DefaultConfig>(const BitVectorState<DefaultConfig> &, size_t, size_t);

gtry::sim::BigInt bitwiseNegation(const gtry::sim::BigInt &v, size_t width);


/**
 * @brief Inserts values represented as a boost::multiprecision::number into a BitVectorState. This does not set or clear any DEFINED flags.
 * 
 * @param vec BitVectorState to insert the bits into
 * @param offset offset of the range to insert bits into
 * @param size size of the range to insert bits into
 * @param v boost::multiprecision::number of the values to insert into the range
 */
template<typename Config>
void insertBigInt(BitVectorState<Config> &vec, size_t offset, size_t size, BigInt v)
{
	// Convert negative numbers to positive 2-complements
	if (v < 0)
		v = bitwiseNegation(v, size) + 1;

	std::vector<typename Config::BaseType> words;
	export_bits(v, std::back_inserter(words), Config::NUM_BITS_PER_BLOCK, false);

	if (size <= Config::NUM_BITS_PER_BLOCK) {
		if (words.empty())
			vec.clearRange(Config::VALUE, offset, size);
		else
			vec.insert(Config::VALUE, offset, size, words[0]);
	} else {
		HCL_ASSERT(offset % Config::NUM_BITS_PER_BLOCK == 0);

		size_t chunk = 0;
		while (chunk < size) {
			size_t chunkSize = std::min<size_t>(Config::NUM_BITS_PER_BLOCK, size-chunk);

			size_t wordIdx = chunk / Config::NUM_BITS_PER_BLOCK;
			if (wordIdx < words.size())
				vec.insertNonStraddling(Config::VALUE, offset + chunk, chunkSize, words[wordIdx]);
			else
				vec.clearRange(Config::VALUE, offset + chunk, chunkSize);

			chunk += chunkSize;
		}
	}
}
extern template void insertBigInt(BitVectorState<DefaultConfig> &, size_t, size_t, BigInt);

using DefaultBitVectorState = BitVectorState<DefaultConfig>;
using ExtendedBitVectorState = BitVectorState<ExtendedConfig>;

DefaultBitVectorState parseBit(char value);
DefaultBitVectorState parseBit(bool value);
DefaultBitVectorState parseBitVector(std::string_view);
ExtendedBitVectorState parseExtendedBit(char value);
ExtendedBitVectorState parseExtendedBit(bool value);
ExtendedBitVectorState parseExtendedBitVector(std::string_view);
DefaultBitVectorState parseBitVector(uint64_t value, size_t width);


template<typename Config>
std::ostream& operator << (std::ostream& s, const BitVectorState<Config>& state)
{
	if ((s.flags() & std::ios_base::hex) && (state.size() % 4 == 0)) {
		for (auto i : utils::Range(state.size()/4)) {
			bool allDefined = true;
			unsigned v = 0;
			for (auto j : utils::Range(4)) {
				v <<= 1;
				allDefined  &= state.get(Config::DEFINED, state.size() - 1 - i*4-j);

				if (state.get(Config::VALUE, state.size() - 1 - i*4-j))
					v |= 1;
			}
			if (allDefined)
				s << v;
			else
				s << 'X';
		}
	} else {
		for (size_t i = state.size()-1; i < state.size(); --i)
		{
			if (!state.get(Config::DEFINED, i))
				s << 'X';
			else if (state.get(Config::VALUE, i))
				s << '1';
			else
				s << '0';
		}
	}
	return s;
}

template<typename Config>
void formatState(std::ostream& s, const BitVectorState<Config>& state, unsigned base, bool dropLeadingZeros)
{
	bool dropping = dropLeadingZeros;
	if ((base == 16) && (state.size() % 4 == 0)) {
		for (auto i : utils::Range(state.size()/4)) {
			bool allDefined = true;
			unsigned v = 0;
			for (auto j : utils::Range(4)) {
				v <<= 1;
				allDefined  &= state.get(Config::DEFINED, state.size() - 1 - i*4-j);

				if (state.get(Config::VALUE, state.size() - 1 - i*4-j))
					v |= 1;
			}
			if (!dropping || v != 0 || i+1 >= state.size()/4) {
				if (allDefined)
					s << v;
				else
					s << 'X';
				dropping = false;
			}
		}
	} else {
		for (size_t i = state.size()-1; i < state.size(); --i)
		{
			if (!state.get(Config::DEFINED, i)) {
				s << 'X';
				dropping = false;
			} else if (state.get(Config::VALUE, i)) {
				s << '1';
				dropping = false;
			} else
				if (!dropping || i == 0)
					s << '0';
		}
	}
}


template<typename Config>
void formatRange(std::ostream& s, const BitVectorState<Config>& state, unsigned base, size_t offset, size_t size)
{
	unsigned logBase = utils::Log2C(base);
	//HCL_ASSERT(size % logBase == 0);
	size_t roundUpSize = (size+logBase-1)/logBase*logBase;

	for (auto i : utils::Range(roundUpSize/logBase)) {
		bool allDefined = true;
		unsigned v = 0;
		for (auto j : utils::Range(logBase)) {
			v <<= 1;
			auto idx = roundUpSize - 1 - i*logBase-j;
			if (idx < size) {
				allDefined  &= state.get(Config::DEFINED, offset + idx);

				if (state.get(Config::VALUE, offset + idx))
					v |= 1;
			}
		}
		if (!allDefined)
			s << 'X';
		else
			if (v < 10)
				s << (unsigned) v;
			else
				s << (char)('A' + (v-10));
	}
}

template<typename Config, typename Functor>
BitVectorState<Config> createBitVectorState(std::size_t numWords, std::size_t wordSize, Functor functor) {
	BitVectorState<Config> state;
	state.resize(numWords * wordSize);

	for (auto i : utils::Range(numWords)) {
		typename Config::BaseType planes[Config::NUM_PLANES];

		functor(i, planes);

		for (auto p : utils::Range(Config::NUM_PLANES))
			if (sizeof(typename Config::BaseType) % wordSize == 0)
				state.insertNonStraddling(p, i * wordSize, wordSize, planes[p]);
			else
				state.insert(p, i * wordSize, wordSize, planes[p]);
	}

	return state;
}

template<typename Functor>
DefaultBitVectorState createDefaultBitVectorState(std::size_t numWords, std::size_t wordSize, Functor functor) {
	return createBitVectorState<DefaultConfig, Functor>(numWords, wordSize, functor);
}

DefaultBitVectorState createDefaultBitVectorState(std::size_t bitWidth, const void *data);
inline DefaultBitVectorState createDefaultBitVectorState(std::span<const std::byte> data) { return createDefaultBitVectorState(data.size() * 8, data.data()); }
DefaultBitVectorState createDefaultBitVectorState(std::size_t bitWidth, size_t value);
DefaultBitVectorState createRandomDefaultBitVectorState(std::size_t bitWidth, std::mt19937 &rng);
DefaultBitVectorState createDefinedRandomDefaultBitVectorState(std::size_t bitWidth, std::mt19937& rng);

ExtendedBitVectorState createExtendedBitVectorState(std::size_t bitWidth, const void *data);
inline ExtendedBitVectorState createExtendedBitVectorState(std::span<const std::byte> data) { return createExtendedBitVectorState(data.size() * 8, data.data()); }

bool operator==(const DefaultBitVectorState &lhs, std::span<const std::byte> rhs);
inline bool operator!=(const DefaultBitVectorState &lhs, std::span<const std::byte> rhs) { return !(lhs == rhs); }
template<typename T> requires (std::is_trivially_copyable_v<T>)
inline bool operator==(const DefaultBitVectorState &lhs, std::span<const T> rhs) { return lhs == std::as_bytes(rhs); }
template<typename T> requires (std::is_trivially_copyable_v<T>)
inline bool operator!=(const DefaultBitVectorState &lhs, std::span<const T> rhs) { return lhs != std::as_bytes(rhs); }

void asData(const DefaultBitVectorState &src, std::span<std::byte> dst, std::span<const std::byte> undefinedFiller);

template<class Config>
void BitVectorState<Config>::resize(size_t size)
{
	m_size = size;
	for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
	{
		m_values[i].resize((size + Config::NUM_BITS_PER_BLOCK - 1) / Config::NUM_BITS_PER_BLOCK);

		if (size % Config::NUM_BITS_PER_BLOCK != 0)
			m_values[i].back() &= utils::bitMaskRange(0, size % Config::NUM_BITS_PER_BLOCK);
	}
}

template<class Config>
void BitVectorState<Config>::clear()
{
	for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
		m_values[i].clear();
}

template<class Config>
bool BitVectorState<Config>::get(typename Config::Plane plane, size_t idx) const
{
	return utils::bitExtract(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::set(typename Config::Plane plane, size_t idx)
{
	utils::bitSet(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::set(typename Config::Plane plane, size_t idx, bool bit)
{
	if (bit)
		utils::bitSet(m_values[plane].data(), idx);
	else
		utils::bitClear(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::clear(typename Config::Plane plane, size_t idx)
{
	utils::bitClear(m_values[plane].data(), idx);
}

template<class Config>
void BitVectorState<Config>::toggle(typename Config::Plane plane, size_t idx)
{
	utils::bitToggle(m_values[plane].data(), idx);
}


template<class Config>
void BitVectorState<Config>::setRange(typename Config::Plane plane, size_t offset, size_t size, bool bit)
{
	typename Config::BaseType content = 0;
	if (bit)
		content = ~content;

	size_t firstWordSize;
	size_t wordOffset = offset / Config::NUM_BITS_PER_BLOCK;
	if (offset % Config::NUM_BITS_PER_BLOCK == 0) {
		firstWordSize = 0;
	} else {
		firstWordSize = std::min(size, Config::NUM_BITS_PER_BLOCK - offset % Config::NUM_BITS_PER_BLOCK);
		insertNonStraddling(plane, offset, firstWordSize, content);
		wordOffset++;
	}

	size_t numFullWords = (size - firstWordSize) / Config::NUM_BITS_PER_BLOCK;
	for (auto i : utils::Range(numFullWords))
		m_values[plane][wordOffset + i] = content;


	size_t trailingWordSize = (size - firstWordSize) % Config::NUM_BITS_PER_BLOCK;
	if (trailingWordSize > 0)
		insertNonStraddling(plane, offset + firstWordSize + numFullWords*Config::NUM_BITS_PER_BLOCK, trailingWordSize, content);
}

template<class Config>
void BitVectorState<Config>::setRange(typename Config::Plane plane, size_t offset, size_t size)
{
	setRange(plane, offset, size, true);
}

template<class Config>
void BitVectorState<Config>::clearRange(typename Config::Plane plane, size_t offset, size_t size)
{
	setRange(plane, offset, size, false);
}

template<class Config>
void BitVectorState<Config>::copyRange(size_t dstOffset, const BitVectorState<Config> &src, size_t srcOffset, size_t size)
{
	if (srcOffset % 8 == 0 && dstOffset % 8 == 0 && size >= 8) {
		size_t bytes = size / 8;
		for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
			memcpy((char*) data((typename Config::Plane) i) + dstOffset/8, (const char*) src.data((typename Config::Plane) i) + srcOffset/8, bytes);

		dstOffset += bytes * 8;
		srcOffset += bytes * 8;
		size -= bytes*8;
	}

	///@todo: Optimize aligned cases (which happen quite frequently!)
	size_t width = size;
	size_t offset = 0;
	while (offset < width) {
		size_t chunkSize = std::min<size_t>(Config::NUM_BITS_PER_BLOCK, width-offset);

		for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
			insert((typename Config::Plane) i, dstOffset + offset, chunkSize,
					(typename Config::BaseType) src.extract((typename Config::Plane) i, srcOffset + offset, chunkSize));

		offset += chunkSize;
	}
}

template<class Config>
bool BitVectorState<Config>::compareRange(size_t dstOffset, const BitVectorState<Config> &src, size_t srcOffset, size_t size) const
{
	if (srcOffset % 8 == 0 && dstOffset % 8 == 0 && size >= 8) {
		size_t bytes = size / 8;
		for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
			if (memcmp((char*) data((typename Config::Plane) i) + dstOffset/8, (const char*) src.data((typename Config::Plane) i) + srcOffset/8, bytes))
				return false;

		dstOffset += bytes * 8;
		srcOffset += bytes * 8;
		size -= bytes*8;
	}

	///@todo: Optimize aligned cases (which happen quite frequently!)
	size_t width = size;
	size_t offset = 0;
	while (offset < width) {
		size_t chunkSize = std::min<size_t>(Config::NUM_BITS_PER_BLOCK, width-offset);

		for (auto i : utils::Range<size_t>(Config::NUM_PLANES)) {
			auto a = src.extract((typename Config::Plane) i, srcOffset + offset, chunkSize);
			auto b = extract((typename Config::Plane) i, dstOffset + offset, chunkSize);
			if (a != b) return false;
		}

		offset += chunkSize;
	}
	return true;
}

template<>
inline bool BitVectorState<DefaultConfig>::compareRange(size_t dstOffset, const BitVectorState<DefaultConfig> &src, size_t srcOffset, size_t size) const
{
	size_t width = size;
	size_t offset = 0;
	while (offset < width) {
		size_t chunkSize = std::min<size_t>(DefaultConfig::NUM_BITS_PER_BLOCK, width-offset);

		auto a_value = src.extract(DefaultConfig::VALUE, srcOffset + offset, chunkSize);
		auto b_value = extract(DefaultConfig::VALUE, dstOffset + offset, chunkSize);

		auto a_defined = src.extract(DefaultConfig::DEFINED, srcOffset + offset, chunkSize);
		auto b_defined = extract(DefaultConfig::DEFINED, dstOffset + offset, chunkSize);


		if (a_defined != b_defined) 
			return false;

		auto differences = a_value ^ b_value;

		if ((differences & a_defined) != 0ull)
			return false;
		
		offset += chunkSize;
	}
	return true;
}

template<>
inline bool BitVectorState<ExtendedConfig>::compareRange(size_t dstOffset, const BitVectorState<ExtendedConfig> &src, size_t srcOffset, size_t size) const
{
	size_t width = size;
	size_t offset = 0;
	while (offset < width) {
		size_t chunkSize = std::min<size_t>(ExtendedConfig::NUM_BITS_PER_BLOCK, width-offset);

		auto a_value = src.extract(ExtendedConfig::VALUE, srcOffset + offset, chunkSize);
		auto b_value = extract(ExtendedConfig::VALUE, dstOffset + offset, chunkSize);

		auto a_defined = src.extract(ExtendedConfig::DEFINED, srcOffset + offset, chunkSize);
		auto b_defined = extract(ExtendedConfig::DEFINED, dstOffset + offset, chunkSize);

		auto a_dont_care = src.extract(ExtendedConfig::DONT_CARE, srcOffset + offset, chunkSize);
		auto b_dont_care = extract(ExtendedConfig::DONT_CARE, dstOffset + offset, chunkSize);

		auto dont_care = a_dont_care | b_dont_care;

		auto a_high_impedance = src.extract(ExtendedConfig::HIGH_IMPEDANCE, srcOffset + offset, chunkSize);
		auto b_high_impedance = extract(ExtendedConfig::HIGH_IMPEDANCE, dstOffset + offset, chunkSize);

		if ((a_high_impedance ^ b_high_impedance) & ~dont_care)
			return false;

		if ((a_defined ^ b_defined) & ~dont_care)
			return false;

		auto differences = a_value ^ b_value;

		if ((differences & a_defined & ~dont_care) != 0ull)
			return false;
		
		offset += chunkSize;
	}
	return true;
}

template<class Config>
typename Config::BaseType *BitVectorState<Config>::data(typename Config::Plane plane)
{
	return m_values[plane].data();
}

template<class Config>
const typename Config::BaseType *BitVectorState<Config>::data(typename Config::Plane plane) const
{
	return m_values[plane].data();
}

template<class Config>
std::span<std::byte> BitVectorState<Config>::asWritableBytes(typename Config::Plane plane)
{
	return std::span<std::byte>((std::byte*)m_values[plane].data(), (m_size + 7)/8);
}

template<class Config>
std::span<const std::byte> BitVectorState<Config>::asBytes(typename Config::Plane plane) const
{
	return std::span<const std::byte>((const std::byte*)m_values[plane].data(), (m_size + 7)/8);
}


template<class Config>
BitVectorState<Config> BitVectorState<Config>::extract(size_t start, size_t size) const
{
	BitVectorState<Config> result;
	result.resize(size);
	if (start % 8 == 0) {
		for (auto i : utils::Range<size_t>(Config::NUM_PLANES))
			memcpy((char*) result.data((typename Config::Plane) i), (char*) data((typename Config::Plane) i) + start/8, (size+7)/8);
	} else
		result.copyRange(0, *this, start, size);

	return result;
}

template<class Config>
inline void BitVectorState<Config>::insert(const BitVectorState& state, size_t offset, size_t size)
{
	const size_t width = size ? size : state.size();
	size_t srcOffset = 0;

	while (srcOffset < width) {
		size_t chunkSize = std::min<size_t>(64, width - srcOffset);

		for (auto i : utils::Range<size_t>(Config::NUM_PLANES)) {
			auto val = state.extractNonStraddling((typename Config::Plane) i, srcOffset, chunkSize);
			insertNonStraddling((typename Config::Plane) i, offset, chunkSize, val);
		}

		offset += chunkSize;
		srcOffset += chunkSize;
	}
}

template<class Config>
inline typename Config::BaseType BitVectorState<Config>::extract(typename Config::Plane plane, size_t offset, size_t size) const
{
	HCL_ASSERT(size <= Config::NUM_BITS_PER_BLOCK);
	const auto* values = &m_values[plane][offset / Config::NUM_BITS_PER_BLOCK];
	const size_t wordOffset = offset % Config::NUM_BITS_PER_BLOCK;

	auto val = values[0];
	val >>= wordOffset;
	if(wordOffset + size > Config::NUM_BITS_PER_BLOCK)
		val |= values[1] << (Config::NUM_BITS_PER_BLOCK - wordOffset);
	val &= utils::bitMaskRange(0, size);

	return val;
}

template<class Config>
typename Config::BaseType BitVectorState<Config>::extractNonStraddling(typename Config::Plane plane, size_t start, size_t size) const
{
	HCL_ASSERT(start % Config::NUM_BITS_PER_BLOCK + size <= Config::NUM_BITS_PER_BLOCK);
	HCL_ASSERT(start / Config::NUM_BITS_PER_BLOCK < m_values[plane].size());
	return utils::bitfieldExtract(m_values[plane][start / Config::NUM_BITS_PER_BLOCK], start % Config::NUM_BITS_PER_BLOCK, size);
}

template<class Config>
inline void BitVectorState<Config>::insert(typename Config::Plane plane, size_t offset, size_t size, typename Config::BaseType value)
{
	HCL_ASSERT(size <= Config::NUM_BITS_PER_BLOCK);
	const size_t wordOffset = offset % Config::NUM_BITS_PER_BLOCK;
	if (wordOffset + size <= Config::NUM_BITS_PER_BLOCK)
	{
		insertNonStraddling(plane, offset, size, value);
		return;
	}

	auto* dst = &m_values[plane][offset / Config::NUM_BITS_PER_BLOCK];
	dst[0] = utils::bitfieldInsert(dst[0], wordOffset, Config::NUM_BITS_PER_BLOCK - wordOffset, value);
	value >>= Config::NUM_BITS_PER_BLOCK - wordOffset;
	dst[1] = utils::bitfieldInsert(dst[1], 0, (wordOffset + size) % Config::NUM_BITS_PER_BLOCK, value);
}

template<class Config>
void BitVectorState<Config>::insertNonStraddling(typename Config::Plane plane, size_t start, size_t size, typename Config::BaseType value)
{
	HCL_ASSERT(start % Config::NUM_BITS_PER_BLOCK + size <= Config::NUM_BITS_PER_BLOCK);
	if (size)
	{
		HCL_ASSERT(start / Config::NUM_BITS_PER_BLOCK < m_values[plane].size());
		auto& op = m_values[plane][start / Config::NUM_BITS_PER_BLOCK];
		op = utils::bitfieldInsert(op, start % Config::NUM_BITS_PER_BLOCK, size, value);
	}
}

template<class Config>
inline std::pair<typename BitVectorState<Config>::iterator, typename BitVectorState<Config>::iterator>
BitVectorState<Config>::range(typename Config::Plane plane, size_t offset, size_t size)
{
	const size_t endOffset = offset + size;
	return {
		iterator{*this, plane, offset, endOffset},
		iterator{*this, plane, endOffset, endOffset}
	};
}

template<class Config>
inline bool BitVectorState<Config>::operator==(const BitVectorState& o) const
{
	if (size() != o.size())	
		return false;

	for (size_t p = 0; p < m_values.size(); ++p)
		for (size_t i = 0; i < m_values[p].size(); ++i) {
			typename Config::BaseType mask = utils::bitMaskRange(0, std::min<size_t>(Config::NUM_BITS_PER_BLOCK, m_size - i*Config::NUM_BITS_PER_BLOCK));
			if ((m_values[p][i] & mask) != (o.m_values[p][i] & mask))
				return false;
		}

	return true;
}

template<class Config>
void BitVectorState<Config>::append(const BitVectorState<Config> &src)
{
	auto offset = size();
	resize(size() + src.size());
	copyRange(offset, src, 0, src.size());
}

extern template class BitVectorState<DefaultConfig>;
extern template class BitVectorState<ExtendedConfig>;



BitVectorState<ExtendedConfig> convertToExtended(const BitVectorState<DefaultConfig> &src);
std::optional<BitVectorState<DefaultConfig>> tryConvertToDefault(const BitVectorState<ExtendedConfig> &src);

}
