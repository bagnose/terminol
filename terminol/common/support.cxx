// vi:noai:sw=4

#include "terminol/common/support.hxx"

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

namespace {

const char * const ESC_TABLE[] = {
    "\033[0m",      // reset

    "\033[1m",      // bold
    "\033[2m",
    "\033[4m",
    "\033[5m",
    "\033[7m",
    "\033[8m",

    "\033[22m",     // cl bold
    "\033[24m",
    "\033[25m",
    "\033[27m",
    "\033[29m",

    "\033[30m",     // fg black
    "\033[31m",     // fg red
    "\033[32m",     // fg green
    "\033[33m",
    "\033[34m",
    "\033[35m",
    "\033[36m",
    "\033[37m",

    "\033[40m",     // bg black
    "\033[41m",     // bg red
    "\033[42m",     // bg green
    "\033[43m",
    "\033[44m",
    "\033[45m",
    "\033[46m",
    "\033[47m"
};

} // namespace {anonymous}

std::ostream &
operator << (std::ostream & ost, Esc e) {
    ost << ESC_TABLE[static_cast<size_t>(e)];
    return ost;
}
