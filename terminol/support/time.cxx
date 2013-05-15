// vi:noai:sw=4

#include "terminol/support/time.hxx"
#include "terminol/support/debug.hxx"

#include <sys/time.h>

Timer::Timer(uint32_t milliseconds) {
    const uint32_t THOUSAND = 1000;
    const uint32_t MILLION  = THOUSAND * THOUSAND;

    uint32_t microseconds = milliseconds * THOUSAND;

    struct timeval tv;
    ENFORCE(::gettimeofday(&tv, nullptr) == 0, "");

    _sec  = tv.tv_sec;
    _usec = tv.tv_usec;

    _sec  += microseconds / MILLION;
    _usec += microseconds % MILLION;

    if (_usec >= MILLION) {
        _sec  += 1;
        _usec -= MILLION;
    }
}

bool Timer::expired() const {
    struct timeval tv;
    ENFORCE(::gettimeofday(&tv, nullptr) == 0, "");

    uint32_t  sec = tv.tv_sec;
    uint32_t usec = tv.tv_usec;

    return sec > _sec || (sec == _sec && usec >= _usec);
}

