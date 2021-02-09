#pragma once

#include <boost/rational.hpp>

namespace hcl::core::hlim {
    using ClockRational = boost::rational<std::uint64_t>;
}