// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__MATH__HXX
#define SUPPORT__MATH__HXX

#include <type_traits>

template <typename A, typename B> auto divideRoundUp(A a, B b) noexcept -> decltype(a / b) {
    static_assert(std::is_integral<A>() && std::is_integral<B>(), "Non integral types.");
    return (a + b - 1) / b;
}

#endif // SUPPORT__MATH__HXX
