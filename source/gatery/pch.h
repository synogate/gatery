#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #define WIN32_LEAN_AND_MEAN
 #define _CRT_SECURE_NO_WARNINGS
 #include <Windows.h>
#endif

#ifndef GTRY_NO_PCH

#ifdef _WIN32
#pragma warning(push, 0)
#pragma warning(disable : 4146) // boost rational "unary minus operator applied to unsigned type, result still unsigned"
#pragma warning(disable : 4018) // boost process environment "'<': signed/unsigned mismatch"
#define _STDFLOAT_ // boost "The contents of <stdfloat> are available only with C++23 or later."
#endif

#include "compat/CoroutineWrapper.h"
#include "compat/boost_pfr.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/for_each.hpp>
#include <boost/hana/ext/std/array.hpp>
#include <boost/hana/ext/std/tuple.hpp>
#include <boost/hana/ext/std/pair.hpp>
#include <boost/hana/fwd/accessors.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/optional.hpp>
#include <boost/rational.hpp>
#include <boost/smart_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/spirit/home/support/container.hpp>
#include <boost/spirit/home/x3.hpp>
#include <boost/stacktrace.hpp>
//#include <boost/process.hpp>
#include <boost/optional.hpp>
#include <boost/container/flat_map.hpp>
#include <boost/multiprecision/debug_adaptor.hpp>
#include <boost/multiprecision/number.hpp>
//#include <boost/multiprecision/mpfi.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/flyweight.hpp>

#include <external/magic_enum.hpp>

#include <bit>
#include <csignal>
#include <cstdint>
#include <concepts>
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
#include <random>
#include <ranges>
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
#include <span>
#include <regex>

#if __has_include(<yaml-cpp/yaml.h>)

#ifdef _WIN32
#pragma warning (push)
#pragma warning (disable : 4251) // yaml-cpp wrong dll interface export for stl
#pragma warning (disable : 4275) // yaml-cpp wrong dll interface export for stl
#endif

# include <yaml-cpp/yaml.h>

#ifdef _WIN32
#pragma warning (pop)
#endif

# define USE_YAMLCPP
#else
# pragma message ("yaml-cpp not found. compiling without config file support")
#endif

namespace boost {
	extern template class rational<std::uint64_t>;
	extern template class basic_format<char>;
	extern template std::basic_string<char> lexical_cast<std::basic_string<char>>(const unsigned long&);
}

namespace std {
	extern template class std::vector<std::uint64_t>;
	extern template class std::span<std::byte>;
	extern template class std::span<const std::byte>;
}

namespace boost::multiprecision {
	//extern template class number<cpp_int_backend<>>;
}



#ifdef _WIN32
#pragma warning(pop)
#endif

#endif
