// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__TIME__HXX
#define COMMON__TIME__HXX

#include <iosfwd>
#include <chrono>
#include <cstdint>

//
// A timer that expires.
//

class Timer {
    using Clock = std::chrono::steady_clock;
    Clock::time_point _endTime;

public:
    explicit Timer(uint32_t milliseconds)
        : _endTime(Clock::now() + std::chrono::duration<int, std::milli>(milliseconds)) {}

    bool expired() const { return std::chrono::steady_clock::now() > _endTime; }
};

#endif // COMMON__TIME__HXX
