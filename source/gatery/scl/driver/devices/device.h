#pragma once

template<typename DevT, typename MapT> requires (sizeof(DevT) == sizeof(MapT))
inline constexpr DevT& device() { return *(DevT*)MapT::ADDRESS; }
