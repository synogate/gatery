#pragma once

#include <boost/rational.hpp>

namespace hcl::core::hlim {
    using ClockRational = boost::rational<std::uint64_t>;

    inline size_t floor(const ClockRational &v) { return v.numerator() / v.denominator(); }

    inline ClockRational operator*(const ClockRational &lhs, int rhs) {
        return lhs * ClockRational(rhs, 1);
    }
    inline ClockRational operator/(const ClockRational &lhs, int rhs) {
        return lhs / ClockRational(rhs, 1);
    }
    inline ClockRational operator*(const ClockRational &lhs, size_t rhs) {
        return lhs * ClockRational(rhs, 1);
    }
    inline ClockRational operator/(const ClockRational &lhs, size_t rhs) {
        return lhs / ClockRational(rhs, 1);
    }
}