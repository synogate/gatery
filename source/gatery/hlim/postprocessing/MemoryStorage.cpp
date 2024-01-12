/*  This file is part of Gatery, a library for circuit design.
	Copyright (C) 2023 Michael Offel, Andreas Ley

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

#include "MemoryStorage.h"

#include <iostream>

namespace gtry::hlim {


MemoryStorage::Initialization MemoryStorage::Initialization::setAllDefinedRandom(size_t size, std::uint64_t offset, uint32_t seed)
{
	std::mt19937 mt(seed);
	MemoryStorage::Initialization res;
	res.initialOverlay.emplace_back();
	res.initialOverlay.back().first = offset;
	auto &data = res.initialOverlay.back().second;
	data.resize(size);
	for (size_t i = 0; i < data.size(); i++) {
		data.setRange(sim::DefaultConfig::DEFINED, 0, size);
		if (mt() & 1) data.set(sim::DefaultConfig::VALUE, i);
		else  data.clear(sim::DefaultConfig::VALUE, i);
	}
	return res;
}


struct Intersection {
	/// Intersection start relative to offset1
	std::uint64_t start1;
	/// Intersection start relative to offset2
	std::uint64_t start2;
	std::uint64_t size;
};

Intersection computeIntersection(std::uint64_t offset1, std::uint64_t size1, std::uint64_t offset2, std::uint64_t size2)
{
	std::uint64_t start = std::max(offset1, offset2);
	std::uint64_t end = std::min(offset1 + size1, offset2 + size2);
	if (end < start)
		return { .size = 0 };
	else
		return {
			.start1 = start - offset1,
			.start2 = start - offset2,
			.size = end - start
		};
}


/**
 * @brief Implements the mechanics of writing potentially undefined data into a (chunk of) memory.
 * @param memory The memory (or chunk thereof) to write to
 * @param dstOffset Offset in the memory to start the write
 * @param value The (potentially undefined) word to write, also defines the size of the write if size == ~0ull
 * @param undefinedWriteEnable Whether the write enable was undefined.
 * @param mask Either empty, or a bit-wise write mask.
 * @param srcOffset The start of the range in value and mask to consider for writing.
 * @param size The amount of bits to write.
 */
void potentiallyUndefinedWrite(sim::DefaultBitVectorState &memory, std::uint64_t dstOffset, 
				const sim::DefaultBitVectorState &value, bool undefinedWriteEnable, const sim::DefaultBitVectorState &mask,
				std::uint64_t srcOffset = 0, std::uint64_t size = ~0ull)
{
	if (size == ~0ull) size = value.size();
	if (mask.size() == 0 && !undefinedWriteEnable)
		memory.copyRange(dstOffset, value, srcOffset, size);
	else {
		// For write masks, look at each bit (todo: optimize)
		for (auto i : utils::Range(size))
			if (!undefinedWriteEnable && mask.get(sim::DefaultConfig::DEFINED, srcOffset + i)) {
				// If the write mask is defined, only copy the bit if it is high
				if (mask.get(sim::DefaultConfig::VALUE, srcOffset + i)) {
					memory.set(sim::DefaultConfig::DEFINED, dstOffset+i, value.get(sim::DefaultConfig::DEFINED, srcOffset + i));
					memory.set(sim::DefaultConfig::VALUE, dstOffset+i, value.get(sim::DefaultConfig::VALUE, srcOffset + i));
				}
			} else
				// If the write mask or writeEnable is undefined, the resulting bit is defined if it was defined before and its value would not change with the write.
				memory.set(sim::DefaultConfig::DEFINED, dstOffset+i, 
									memory.get(sim::DefaultConfig::DEFINED, dstOffset+i) && value.get(sim::DefaultConfig::DEFINED, srcOffset + i) &&
												(memory.get(sim::DefaultConfig::VALUE, dstOffset+i) == value.get(sim::DefaultConfig::VALUE, srcOffset + i)));
	}
}




MemoryStorageDense::MemoryStorageDense(std::uint64_t size, const Initialization &initialization)
{ 
	m_memory.resize(size);
	if (initialization.background) {
		std::span<const uint8_t> backgroundSpan;
		boost::iostreams::mapped_file_source mappedBackgroundFile;
		if (std::holds_alternative<std::span<uint8_t>>(*initialization.background))
			backgroundSpan = std::get<std::span<uint8_t>>(*initialization.background);
		else {
			mappedBackgroundFile.open(boost::filesystem::path{std::get<std::filesystem::path>(*initialization.background)});
			backgroundSpan = std::span<const uint8_t>{ (const uint8_t*) mappedBackgroundFile.data(), mappedBackgroundFile.size() };
		}

		HCL_ASSERT(backgroundSpan.size() <= m_memory.size()*8);
		auto converted = sim::createDefaultBitVectorState(backgroundSpan.size() * 8, backgroundSpan.data());
		m_memory.copyRange(0, converted, 0, std::min(m_memory.size(), converted.size()));
	}

	for (const auto &v : initialization.initialOverlay) {
		HCL_ASSERT(v.first + v.second.size() <= m_memory.size());
		m_memory.copyRange(v.first, v.second, 0, v.second.size());
	}
}

void MemoryStorageDense::read(sim::DefaultBitVectorState &dst, std::uint64_t offset, std::uint64_t size) const
{
	dst.resize(size);
	dst.copyRange(0, m_memory, offset, size);
}

void MemoryStorageDense::write(std::uint64_t offset, const sim::DefaultBitVectorState &value, bool undefinedWriteEnable, const sim::DefaultBitVectorState &mask)
{
	potentiallyUndefinedWrite(m_memory, offset, value, undefinedWriteEnable, mask);
}

void MemoryStorageDense::setAllUndefined()
{
	m_memory.clearRange(sim::DefaultConfig::DEFINED, 0, m_memory.size());
}

MemoryStorageSparse::MemoryStorageSparse(std::uint64_t size, const Initialization &initialization) : m_size(size)
{
	if (initialization.background) {
		if (std::holds_alternative<std::span<uint8_t>>(*initialization.background))
			m_background = std::get<std::span<uint8_t>>(*initialization.background);
		else {
			m_mappedBackgroundFile.open(boost::filesystem::path{std::get<std::filesystem::path>(*initialization.background)});
			m_background = std::span<const uint8_t>{ (const uint8_t*) m_mappedBackgroundFile.data(), m_mappedBackgroundFile.size() };
		}
	}

	for (const auto &v : initialization.initialOverlay)
		write(v.first, v.second, false, {});
}


void MemoryStorageSparse::read(sim::DefaultBitVectorState &dst, std::uint64_t offset, std::uint64_t size) const
{
	dst.resize(size);
	dst.clearRange(sim::DefaultConfig::DEFINED, 0, size);

	if (size == 0) return;

	populateFromBackground(offset, dst);
	
	forEachOverlapping(offset, size, [&dst, offset, size](OverlayMap::const_iterator it){
		auto intersection = computeIntersection(offset, size, it->first, it->second.size());
		HCL_ASSERT(intersection.size > 0);
		dst.copyRange(intersection.start1, it->second, intersection.start2, intersection.size);
	});
}

void MemoryStorageSparse::write(std::uint64_t offset, const sim::DefaultBitVectorState &value, bool undefinedWriteEnable, const sim::DefaultBitVectorState &mask)
{
	if (value.size() == 0) return;

	OverlayMap::iterator last = m_overlay.end();
	OverlayMap::iterator first = m_overlay.end();

	auto handleChunk = [this, &first, &last, offset, &value, &undefinedWriteEnable, &mask](OverlayMap::iterator it){
		auto & [chunkOffset, chunkData] = *it;
		if (last == m_overlay.end()) {
			// This is the first chunk we encounter.

			// Enlarge this chunk to fully hold the end of the new write if necessary
			if (offset+value.size() > chunkOffset + chunkData.size()) {
				auto oldSize = chunkData.size();
				chunkData.resize(offset + value.size() - chunkOffset);

				// The new chunk must be prepopulated with the background data to correctly handle masked and undefined writes
				chunkData.clearRange(sim::DefaultConfig::DEFINED, oldSize, chunkData.size() - oldSize);
				populateFromBackground(chunkOffset + oldSize, chunkData, oldSize, chunkData.size() - oldSize);
			}

			auto intersection = computeIntersection(offset, value.size(), chunkOffset, chunkData.size());
			HCL_ASSERT(intersection.size > 0);
			
			// Perform write
			potentiallyUndefinedWrite(chunkData, intersection.start2, value, undefinedWriteEnable, mask, intersection.start1, intersection.size);

			last = it;
			first = it;
		} else {
			// We found another chunk that overlaps.
			// The last chunk must be merged with this one (because we are moving in reverse through address space)

			auto & [lastChunkOffset, lastChunkData] = *last;

			std::uint64_t previousSize = chunkData.size();
			std::uint64_t offsetLastInCurrent = lastChunkOffset - chunkOffset;

			// Resize to encompass the new chunk and copy data 
			chunkData.resize(lastChunkOffset + lastChunkData.size() - chunkOffset);
			chunkData.clearRange(sim::DefaultConfig::DEFINED, previousSize, chunkData.size() - previousSize);
			chunkData.copyRange(offsetLastInCurrent, lastChunkData, 0, lastChunkData.size());

			// There may be a gap between the two, which we need to populate from background
			std::uint64_t gapStart = previousSize;
			std::uint64_t gapSize = offsetLastInCurrent - previousSize;
			populateFromBackground(chunkOffset + gapStart, chunkData, gapStart, gapSize);

			// Reapply the write. Although it wouldn't hurt, we do not need to reapply the write to the data copied from lastChunk
			auto intersection = computeIntersection(offset, value.size(), chunkOffset, offsetLastInCurrent);
			potentiallyUndefinedWrite(chunkData, intersection.start2, value, undefinedWriteEnable, mask, intersection.start1, intersection.size);

			last = it;
		}
	};

	forEachOverlapping(offset, value.size(), handleChunk);

	// Check if the last one handled the start of the write
	if (last == m_overlay.end() || !(last->first <= offset && last->first + last->second.size() > offset)) {
		// Insert a chunk with the missing range and have it fused by calling handleChunk again.

		size_t newChunkOffset = offset;
		size_t newChunkSize = value.size();
		if (last != m_overlay.end())
			newChunkSize = last->first - newChunkOffset;
		
		sim::DefaultBitVectorState newChunkData;
		newChunkData.resize(newChunkSize);
		populateFromBackground(newChunkOffset, newChunkData);

		auto [it, _] = m_overlay.insert({offset, std::move(newChunkData)});
		handleChunk(it);
	}

	// Delete all chunks that were fused
	if (first != last) {
		auto deleteBegin = last; ++deleteBegin;
		auto deleteEnd = first; ++deleteEnd;

		m_overlay.erase(deleteBegin, deleteEnd);
	}
}


template<typename Functor>
void MemoryStorageSparse::forEachOverlapping(std::uint64_t offset, std::uint64_t size, Functor functor) const
{
	auto end = offset + size;
	// Find the first element, that is fully after the considered range (or the end iterator).
	auto firstNotAffected = m_overlay.lower_bound(end);
	// Create a reverse iterator (this implicitely points to the previous element).
	auto lastAffected = std::reverse_iterator(firstNotAffected);

	// Walk backwards (using reverse iterator) to collect until we hit an element fully before the range we consider.
	for (auto it = lastAffected; it != m_overlay.rend(); ++it) {
		if (it->first + it->second.size() <= offset)
			break;
		// Remember to correct the implicit offset by one when converting from reverse back to forward iterator.
		auto forwardIterator = it.base(); --forwardIterator;
		functor(forwardIterator);
	}
}

void MemoryStorageSparse::populateFromBackground(std::uint64_t backgroundOffset, sim::DefaultBitVectorState &value, std::uint64_t valueOffset, std::uint64_t valueSize) const
{
	if (valueSize == ~0ull)
		valueSize = value.size();

	auto intersection = computeIntersection(backgroundOffset, valueSize, 0, m_background.size()*8);
	if (intersection.size > 0) {
		auto paddedRange = m_background.subspan(intersection.start2 / 8, (intersection.size + intersection.start2 % 8 + 7) / 8);
		auto paddedData = sim::createDefaultBitVectorState(paddedRange.size() * 8, paddedRange.data());
		value.copyRange(valueOffset+intersection.start1, paddedData, intersection.start2 % 8, intersection.size);
	}

}

void MemoryStorageSparse::setAllUndefined()
{
	m_background = {};
	m_overlay.clear();
}


}
