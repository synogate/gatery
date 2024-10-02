#include "gatery/pch.h"
#include <boost/rational.hpp>

namespace boost {
	template class rational<std::uint64_t>;
	template class basic_format<char>;
	template std::basic_string<char> lexical_cast<std::basic_string<char>>(const unsigned long&);
}

namespace std {
	template class std::vector<std::uint64_t>;
	template class std::span<std::byte>;
	template class std::span<const std::byte>;
}

namespace boost::multiprecision {
	//template class number<cpp_int_backend<>>;
}
