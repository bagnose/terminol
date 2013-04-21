// vi:noai:sw=4

#include "terminol/common/support.hxx"

#include <sys/time.h>

Timer::Timer(uint32_t milliseconds) {
    struct timeval tv;
    ENFORCE(::gettimeofday(&tv, nullptr) == 0, "");

    _sec  = tv.tv_sec;
    _usec = tv.tv_usec;

    _sec  += milliseconds / 1000;
    _usec += milliseconds % 1000;

    if (_usec >= 1000000) {
        _sec  += 1;
        _usec -= 1000000;
    }
}

bool Timer::expired() const {
    struct timeval tv;
    ENFORCE(::gettimeofday(&tv, nullptr) == 0, "");

    uint32_t  sec = tv.tv_sec;
    uint32_t usec = tv.tv_usec;

    return sec > _sec || (sec == _sec && usec >= _usec);
}
