#pragma once


#include <vector>
#include <array>

namespace mhdl::core::sim {
    
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

        size_t allocate(unsigned size);
        void flushBuckets();
        inline size_t getTotalSize() const { return m_totalSize; }
    protected:
        struct Bucket {
            size_t offset = 0;
            unsigned remaining = 0;
        };
        std::array<Bucket, NUM_BUCKETS> m_buckets;
        size_t m_totalSize = 0;
};

}
