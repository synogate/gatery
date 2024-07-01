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
#include "gatery/pch.h"
#include "BitVectorState.h"

namespace gtry::sim {

template class BitVectorState<DefaultConfig>;
template class BitVectorState<ExtendedConfig>;

DefaultBitVectorState createDefaultBitVectorState(std::size_t bitWidth, const void *data) {
	BitVectorState<DefaultConfig> state;
	state.resize(bitWidth);
	state.setRange(DefaultConfig::DEFINED, 0, bitWidth);
	memcpy(state.data(DefaultConfig::VALUE), data, (bitWidth + 7) / 8);
	return state;
}

DefaultBitVectorState createDefaultBitVectorState(std::size_t bitWidth, size_t value)
{
	BitVectorState<DefaultConfig> state;
	state.resize(bitWidth);
	state.setRange(DefaultConfig::DEFINED, 0, bitWidth);
	state.clearRange(DefaultConfig::VALUE, 0, bitWidth);
	state.insertNonStraddling(DefaultConfig::VALUE, 0, sizeof(value) * 8, value);
	return state;
}

DefaultBitVectorState createRandomDefaultBitVectorState(std::size_t bitWidth, std::mt19937 &rng)
{
	std::uniform_int_distribution<DefaultConfig::BaseType> random;

	BitVectorState<DefaultConfig> state;
	state.resize(bitWidth);
	for (size_t i = 0; i < DefaultConfig::NUM_PLANES; i++)
		for (size_t j = 0; j < state.getNumBlocks(); j++)
			state.data((DefaultConfig::Plane) i)[j] = random(rng);
	return state;
}

DefaultBitVectorState createDefinedRandomDefaultBitVectorState(std::size_t bitWidth, std::mt19937 &rng)
{
	std::uniform_int_distribution<DefaultConfig::BaseType> random;

	BitVectorState<DefaultConfig> state;
	state.resize(bitWidth);
	
	for (size_t j = 0; j < state.getNumBlocks(); j++)
	{
		state.data(DefaultConfig::VALUE)[j] = random(rng);
		state.data(DefaultConfig::DEFINED)[j] = ~0ull; //64bit mask
	}
	return state;
}

ExtendedBitVectorState createExtendedBitVectorState(std::size_t bitWidth, const void *data) 
{
	BitVectorState<ExtendedConfig> state;
	state.resize(bitWidth);
	state.setRange(ExtendedConfig::DEFINED, 0, bitWidth);
	memcpy(state.data(ExtendedConfig::VALUE), data, (bitWidth + 7) / 8);
	state.clearRange(ExtendedConfig::DONT_CARE, 0, bitWidth);
	state.clearRange(ExtendedConfig::HIGH_IMPEDANCE, 0, bitWidth);
	return state;
}

bool operator==(const DefaultBitVectorState &lhs, std::span<const std::byte> rhs)
{
	if (lhs.size() != rhs.size() * 8)
		throw std::runtime_error("The array that is to be compared to the simulation signal has the wrong size!");

	// If anything in the lhs is undefined, the comparison is always false.
	if (!sim::allDefined<DefaultConfig>(lhs))
		return false;

	size_t numFullWords = rhs.size() / sizeof(sim::DefaultConfig::BaseType);
	size_t remainingBytes = rhs.size() - numFullWords * sizeof(sim::DefaultConfig::BaseType);

	const sim::DefaultConfig::BaseType *srcWords = (const sim::DefaultConfig::BaseType *) rhs.data();

	// compare full words directly
	for (auto wordIdx : utils::Range(numFullWords))
		if (lhs.data(sim::DefaultConfig::VALUE)[wordIdx] != srcWords[wordIdx]) return false;

	// For partial words, create a bit mask to mask the differences, then check if any differences remain after the masking.
	if (remainingBytes > 0) {
		auto lastStateWord = lhs.data(sim::DefaultConfig::VALUE)[numFullWords];
		auto lastCompareWord = srcWords[numFullWords];
		auto difference = lastStateWord ^ lastCompareWord;
		sim::DefaultConfig::BaseType mask = ~0ull;
		mask >>= (sizeof(sim::DefaultConfig::BaseType) - remainingBytes) * 8;
		difference &= mask;
		if (difference) 
			return false;
	}
	return true;
}


void asData(const DefaultBitVectorState &src, std::span<std::byte> dst, std::span<const std::byte> undefinedFiller)
{
	HCL_DESIGNCHECK(dst.size()*8 == src.size());

	std::byte *value = (std::byte *) src.data(DefaultConfig::VALUE);
	std::byte *defined = (std::byte *) src.data(DefaultConfig::DEFINED);

	for (size_t j = 0; j < dst.size(); j++) {
		auto def = defined[j];
		
		std::byte filler = (std::byte)'X';
		if (!undefinedFiller.empty()) filler = undefinedFiller[j % undefinedFiller.size()];

		dst[j] = (value[j] & def) | (filler & ~def);
	}
}


template void insertBigInt(BitVectorState<DefaultConfig> &, size_t, size_t, BigInt);
template BigInt extractBigInt<DefaultConfig>(const BitVectorState<DefaultConfig> &, size_t, size_t);

gtry::sim::BigInt bitwiseNegation(const gtry::sim::BigInt &v, size_t width)
{
	// Export and reimport so it can't track the sign.
	std::vector<std::uint64_t> words;
	export_bits(v, std::back_inserter(words), 64, false);
	for (auto &elem : words)
		elem = ~elem;

	while (words.size() < width/64)
		words.push_back(~0ull);
	
	gtry::sim::BigInt result;
   	import_bits(result, words.begin(), words.end(), 64, false);
	return result;
}


DefaultBitVectorState parseBit(char value)
{
	HCL_DESIGNCHECK(value == '0' || value == '1' || value == 'x' || value == 'X');

	sim::DefaultBitVectorState ret;
	ret.resize(1);
	ret.set(sim::DefaultConfig::VALUE, 0, value != '0');
	ret.set(sim::DefaultConfig::DEFINED, 0, value != 'x' && value != 'X');
	return ret;
}

DefaultBitVectorState parseBit(bool value)
{
	sim::DefaultBitVectorState ret;
	ret.resize(1);
	ret.set(sim::DefaultConfig::VALUE, 0, value);
	ret.set(sim::DefaultConfig::DEFINED, 0, true);
	return ret;
}

DefaultBitVectorState parseBitVector(std::string_view value)
{
	using namespace boost::spirit::x3;

	sim::DefaultBitVectorState ret;

	auto parseWidth = [&](auto& ctx) {
		if (_attr(ctx))
		{
			const size_t width = *_attr(ctx);
			ret.resize(width);
			ret.setRange(sim::DefaultConfig::VALUE, 0, width, false);
			ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
		}
	};

	auto parseHex = [&](size_t bps, auto& ctx) {

		std::string_view num = _attr(ctx);
		if (ret.size() == 0)
		{
			ret.resize(num.size() * bps);
		}
		else
			HCL_DESIGNCHECK_HINT(ret.size() >= num.size() * bps, "string UInt constant width is to small for its value");

		for (size_t i = 0; i < num.size(); ++i)
		{
			uint8_t value = 0;
			uint8_t defined = 0xFF;
			if (num[i] >= '0' && num[i] <= '9')
				value = num[i] - '0';
			else if (num[i] >= 'a' && num[i] <= 'f')
				value = num[i] - 'a' + 10;
			else if (num[i] >= 'A' && num[i] <= 'F')
				value = num[i] - 'A' + 10;
			else
				defined = 0;

			size_t dstIdx = num.size() - 1 - i;
			ret.insertNonStraddling(sim::DefaultConfig::VALUE, dstIdx * bps, bps, value);
			ret.insertNonStraddling(sim::DefaultConfig::DEFINED, dstIdx * bps, bps, defined);
		}
	};

	auto parseDec = [&](auto& ctx) {
		std::string_view numStr = _attr(ctx);
		uint64_t num = std::strtoull(numStr.data(), nullptr, 10);
		uint64_t width = utils::Log2C(num + 1);

		if (ret.size() == 0)
			ret.resize(width);
		HCL_DESIGNCHECK_HINT(ret.size() >= width, "string UInt constant width is to small for its value");

		ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
		for (size_t i = 0; i < width; ++i)
			ret.set(sim::DefaultConfig::VALUE, i, (num & (1ull << i)) != false);
	};

	auto parseString = [&](auto& ctx) {
		std::string_view str = _attr(ctx);
		uint64_t width = str.length() * 8;

		if (ret.size() == 0)
			ret.resize(width);
		HCL_DESIGNCHECK_HINT(ret.size() >= width, "string UInt constant width is to small for its value");

		ret.setRange(sim::DefaultConfig::DEFINED, 0, width, true);
		for (size_t i = 0; i < width; ++i) {
			char c = str[i/8];
			bool bit = c & (1 << (i % 8));
			ret.set(sim::DefaultConfig::VALUE, i, bit);
		}
	};

	try {
		parse(value.begin(), value.end(),
			(-uint_)[parseWidth] > (
				(char_('s') > (*char_)[parseString]) |
				(char_('x') > (*char_("0-9a-fA-FxX"))[std::bind(parseHex, 4, std::placeholders::_1)]) |
				(char_('o') > (*char_("0-7xX"))[std::bind(parseHex, 3, std::placeholders::_1)]) |
				(char_('b') > (*char_("0-1xX"))[std::bind(parseHex, 1, std::placeholders::_1)]) |
				(char_('d') > (*char_("0-9"))[parseDec])
			) > eoi
		);
	}
	catch (const expectation_failure<std::string_view::iterator>&)
	{
		HCL_DESIGNCHECK_HINT(false, std::string("parsing of UInt literal failed (32xF, b0, ...), got: ") + std::string(value));
	}

	return ret;
}



ExtendedBitVectorState parseExtendedBit(char value)
{
	HCL_DESIGNCHECK(value == '0' || value == '1' || value == 'x' || value == 'X' || value == '-' || value == 'z' || value == 'Z');

	sim::ExtendedBitVectorState ret;
	ret.resize(1);
	ret.set(sim::ExtendedConfig::DEFINED, 0, false);
	ret.set(sim::ExtendedConfig::DONT_CARE, 0, false);
	ret.set(sim::ExtendedConfig::HIGH_IMPEDANCE, 0, false);
	switch (value) {
		case '0':
		case '1':
			ret.set(sim::ExtendedConfig::VALUE, 0, value != '0');
			ret.set(sim::ExtendedConfig::DEFINED, 0, true);
		break;
		case '-':
			ret.set(sim::ExtendedConfig::DONT_CARE, 0, true);
		break;
		case 'x':
		case 'X':
			//ret.set(sim::ExtendedConfig::DEFINED, 0, false);
		break;
		case 'z':
		case 'Z':
			ret.set(sim::ExtendedConfig::HIGH_IMPEDANCE, 0, true);
		break;
		default:
			HCL_DESIGNCHECK_HINT(false, "Invalid character!");
	}
	return ret;
}

ExtendedBitVectorState parseExtendedBit(bool value)
{
	sim::ExtendedBitVectorState ret;
	ret.resize(1);
	ret.set(sim::ExtendedConfig::VALUE, 0, value);
	ret.set(sim::ExtendedConfig::DEFINED, 0, true);
	ret.set(sim::ExtendedConfig::DONT_CARE, 0, false);
	ret.set(sim::ExtendedConfig::HIGH_IMPEDANCE, 0, false);
	return ret;
}

ExtendedBitVectorState parseExtendedBitVector(std::string_view value)
{
	using namespace boost::spirit::x3;

	sim::ExtendedBitVectorState ret;

	auto parseWidth = [&](auto& ctx) {
		if (_attr(ctx))
		{
			const size_t width = *_attr(ctx);
			ret.resize(width);
			ret.setRange(sim::ExtendedConfig::VALUE, 0, width, false);
			ret.setRange(sim::ExtendedConfig::DEFINED, 0, width, true);
			ret.setRange(sim::ExtendedConfig::DONT_CARE, 0, width, false);
			ret.setRange(sim::ExtendedConfig::HIGH_IMPEDANCE, 0, width, false);
		}
	};

	auto parseHex = [&](size_t bps, auto& ctx) {

		std::string_view num = _attr(ctx);
		if (ret.size() == 0)
		{
			ret.resize(num.size() * bps);
		}
		else
			HCL_DESIGNCHECK_HINT(ret.size() >= num.size() * bps, "string UInt constant width is to small for its value");

		for (size_t i = 0; i < num.size(); ++i)
		{
			uint8_t value = 0;
			uint8_t dont_care = 0;
			uint8_t high_impedance = 0;
			uint8_t defined = 0xFF;
			if (num[i] >= '0' && num[i] <= '9')
				value = num[i] - '0';
			else if (num[i] >= 'a' && num[i] <= 'f')
				value = num[i] - 'a' + 10;
			else if (num[i] >= 'A' && num[i] <= 'F')
				value = num[i] - 'A' + 10;
			else if (num[i] == '-')
				dont_care = 0xFF;
			else if (num[i] == 'z' || num[i] == 'Z')
				high_impedance = 0xFF;
			else
				defined = 0;

			size_t dstIdx = num.size() - 1 - i;
			ret.insertNonStraddling(sim::ExtendedConfig::VALUE, dstIdx * bps, bps, value);
			ret.insertNonStraddling(sim::ExtendedConfig::DEFINED, dstIdx * bps, bps, defined);
			ret.insertNonStraddling(sim::ExtendedConfig::DONT_CARE, dstIdx * bps, bps, dont_care);
			ret.insertNonStraddling(sim::ExtendedConfig::HIGH_IMPEDANCE, dstIdx * bps, bps, high_impedance);
		}
	};

	auto parseDec = [&](auto& ctx) {
		std::string_view numStr = _attr(ctx);
		uint64_t num = std::strtoull(numStr.data(), nullptr, 10);
		uint64_t width = utils::Log2C(num + 1);

		if (ret.size() == 0)
			ret.resize(width);
		HCL_DESIGNCHECK_HINT(ret.size() >= width, "string UInt constant width is to small for its value");

		ret.setRange(sim::ExtendedConfig::DEFINED, 0, width, true);
		for (size_t i = 0; i < width; ++i)
			ret.set(sim::ExtendedConfig::VALUE, i, (num & (1ull << i)) != false);
	};

	try {
		parse(value.begin(), value.end(),
			(-uint_)[parseWidth] > (
				(char_('x') > (*char_("0-9a-fA-FxXzZ\\-"))[std::bind(parseHex, 4, std::placeholders::_1)]) |
				(char_('o') > (*char_("0-7xXzZ\\-"))[std::bind(parseHex, 3, std::placeholders::_1)]) |
				(char_('b') > (*char_("0-1xXzZ\\-"))[std::bind(parseHex, 1, std::placeholders::_1)]) |
				(char_('d') > (*char_("0-9"))[parseDec])
			) > eoi
		);
	}
	catch (const expectation_failure<std::string_view::iterator>&)
	{
		HCL_DESIGNCHECK_HINT(false, "parsing of UInt literal failed (32xF, b0, ...)");
	}

	return ret;
}


DefaultBitVectorState parseBitVector(uint64_t value, size_t width)
{
	//HCL_ASSERT(width <= sizeof(size_t) * 8);

	sim::DefaultBitVectorState ret;
	ret.resize(width);
	ret.clearRange(sim::DefaultConfig::VALUE, 0, width);
	ret.setRange(sim::DefaultConfig::DEFINED, 0, width);

	ret.insertNonStraddling(sim::DefaultConfig::VALUE, 0, std::min(sizeof(size_t) * 8, width), value);
	return ret;
}

BitVectorState<ExtendedConfig> convertToExtended(const BitVectorState<DefaultConfig> &src)
{
	HCL_ASSERT(DefaultConfig::NUM_BITS_PER_BLOCK == ExtendedConfig::NUM_BITS_PER_BLOCK);

	BitVectorState<ExtendedConfig> result;
	result.resize(src.size());

	for (size_t offset = 0; offset < src.size(); offset += ExtendedConfig::NUM_BITS_PER_BLOCK) {
		size_t chunkSize = std::min<size_t>(ExtendedConfig::NUM_BITS_PER_BLOCK, src.size()-offset);
		result.insert(ExtendedConfig::VALUE, offset, chunkSize, src.extract(DefaultConfig::VALUE, offset, chunkSize));
		result.insert(ExtendedConfig::DEFINED, offset, chunkSize, src.extract(DefaultConfig::DEFINED, offset, chunkSize));
	}
	return result;
}

std::optional<BitVectorState<DefaultConfig>> tryConvertToDefault(const BitVectorState<ExtendedConfig> &src)
{
	HCL_ASSERT(DefaultConfig::NUM_BITS_PER_BLOCK == ExtendedConfig::NUM_BITS_PER_BLOCK);
	for (size_t offset = 0; offset < src.size(); offset += ExtendedConfig::NUM_BITS_PER_BLOCK) {
		size_t chunkSize = std::min<size_t>(ExtendedConfig::NUM_BITS_PER_BLOCK, src.size()-offset);

		if (src.extract(ExtendedConfig::DONT_CARE, offset, chunkSize)) return {};
		if (src.extract(ExtendedConfig::HIGH_IMPEDANCE, offset, chunkSize)) return {};
	}

	std::optional<BitVectorState<DefaultConfig>> result;
	result.emplace();
	result->resize(src.size());

	for (size_t offset = 0; offset < src.size(); offset += ExtendedConfig::NUM_BITS_PER_BLOCK) {
		size_t chunkSize = std::min<size_t>(ExtendedConfig::NUM_BITS_PER_BLOCK, src.size()-offset);
		result->insert(DefaultConfig::VALUE, offset, chunkSize, src.extract(ExtendedConfig::VALUE, offset, chunkSize));
		result->insert(DefaultConfig::DEFINED, offset, chunkSize, src.extract(ExtendedConfig::DEFINED, offset, chunkSize));
	}
	return result;
}

}
