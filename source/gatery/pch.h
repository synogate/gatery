#ifdef _WIN32
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #define WIN32_LEAN_AND_MEAN
 #define _CRT_SECURE_NO_WARNINGS
 #include <Windows.h>
#endif

#ifndef GTRY_NO_PCH

#pragma warning(push, 0)
#pragma warning(disable : 4146) // boost rational "unary minus operator applied to unsigned type, result still unsigned"

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
#include <boost/pfr.hpp>
#include <boost/optional.hpp>

#include "utils/CoroutineWrapper.h"

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

# include <external/magic_enum.hpp>
# define USE_YAMLCPP
#else
# pragma message ("yaml-cpp not found. compiling without config file support")
#endif

extern template class boost::rational<std::uint64_t>;

#pragma warning(pop)

#endif
