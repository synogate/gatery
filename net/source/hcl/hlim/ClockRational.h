#pragma once

#include <boost/rational.hpp>

namespace hcl::core::hlim {
    using ClockRational = boost::rational<std::uint64_t>;

    inline bool clockLess(const ClockRational &lhs, const ClockRational &rhs) {
        return lhs.numerator() * rhs.denominator() < rhs.numerator() * lhs.denominator();
    }

    inline bool clockMore(const ClockRational &lhs, const ClockRational &rhs) {
        return lhs.numerator() * rhs.denominator() > rhs.numerator() * lhs.denominator();
    }

    inline bool operator<(const ClockRational &lhs, const ClockRational &rhs) {
        return lhs.numerator() * rhs.denominator() < rhs.numerator() * lhs.denominator();
    }
    inline bool operator>(const ClockRational &lhs, const ClockRational &rhs) {
        return lhs.numerator() * rhs.denominator() > rhs.numerator() * lhs.denominator();
    }

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


    inline ClockRational operator/(int lhs, const ClockRational &rhs) {
        return ClockRational(lhs, 1) / rhs;
    }
    inline ClockRational operator/(size_t lhs, const ClockRational &rhs) {
        return ClockRational(lhs, 1) / rhs;
    }
}