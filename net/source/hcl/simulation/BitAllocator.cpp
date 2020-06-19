#include "BitAllocator.h"

#include "../utils/BitManipulation.h"

namespace hcl::core::sim {

size_t BitAllocator::allocate(unsigned size) {
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
