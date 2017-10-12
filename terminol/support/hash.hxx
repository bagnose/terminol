// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__HASH__HXX
#define SUPPORT__HASH__HXX

#include <numeric>

template <class T> struct SDBM {
    using Type = T;
    T operator () (T val, uint8_t next) noexcept {
        return static_cast<T>(next) + (val << 6) + (val << 16) - val;
    }
};

template <class A> typename A::Type hash(const void * buffer,
                                         size_t       length) noexcept {
    auto buf = static_cast<const uint8_t *>(buffer);
    return std::accumulate(buf, buf + length, typename A::Type(0), A());
}

#endif // SUPPORT__HASH__HXX
