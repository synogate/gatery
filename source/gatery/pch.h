#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #define WIN32_LEAN_AND_MEAN
 #include <Windows.h>
#endif

#ifndef GTRY_NO_PCH

#pragma warning(push, 0)

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/fwd/accessors.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/rational.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/spirit/home/support/container.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/stacktrace.hpp>

#include <bit>
#include <coroutine>
#include <csignal>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <immintrin.h>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <stack>
#include <stdexcept>
#include <string_view>
#include <string.h>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <yaml-cpp/yaml.h>


extern template boost::rational<std::uint64_t>;

#pragma warning(pop)

#endif
