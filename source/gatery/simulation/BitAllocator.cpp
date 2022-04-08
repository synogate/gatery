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
#include "BitAllocator.h"

#include "../utils/BitManipulation.h"

namespace gtry::sim {

size_t BitAllocator::allocate(size_t size) {
	if (size == 0)
		return 0ull;
	
	if (size <= 32) {
		size = utils::nextPow2(size);
		
		unsigned bucket;
		switch (size) {
			case 1: bucket = BUCKET_1; break;
			case 2: bucket = BUCKET_2; break;
			case 4: bucket = BUCKET_4; break;
			case 8: bucket = BUCKET_8; break;
			case 16: bucket = BUCKET_16; break;
			case 32: bucket = BUCKET_32; break;
		}
		
		if (m_buckets[bucket].remaining == 0) {
			m_buckets[bucket].offset = m_totalSize;
			m_totalSize += 64;
			m_buckets[bucket].remaining = 64 / size;
		}
		size_t offset = m_buckets[bucket].offset;
		m_buckets[bucket].offset += size;
		m_buckets[bucket].remaining--;
		return offset;
	} else {
		size = (size + 63ull) & ~63ull;
		size_t offset = m_totalSize;
		m_totalSize += size;
		return offset;
	}
}

void BitAllocator::flushBuckets() {
	for (auto i : utils::Range<unsigned>(NUM_BUCKETS)) {
		m_buckets[i].remaining = 0;
	}
}

}
