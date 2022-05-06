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


#include <vector>
#include <array>

namespace gtry::sim {
	
class BitAllocator {
	public:
		enum {
			BUCKET_1,
			BUCKET_2,
			BUCKET_4,
			BUCKET_8,
			BUCKET_16,
			BUCKET_32,
			NUM_BUCKETS
		};

		size_t allocate(size_t size);
		void flushBuckets();
		inline size_t getTotalSize() const { return m_totalSize; }
	protected:
		struct Bucket {
			size_t offset = 0;
			size_t remaining = 0;
		};
		std::array<Bucket, NUM_BUCKETS> m_buckets;
		size_t m_totalSize = 0;
};

}
