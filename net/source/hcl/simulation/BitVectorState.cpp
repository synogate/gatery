#include "BitVectorState.h"

namespace hcl::core::sim {

DefaultBitVectorState createDefaultBitVectorState(std::size_t size, const void *data) {
    BitVectorState<DefaultConfig> state;
    state.resize(size*8);
    state.setRange(DefaultConfig::DEFINED, 0, size*8);
    memcpy(state.data(DefaultConfig::VALUE), data, size);
    return state;
}

}