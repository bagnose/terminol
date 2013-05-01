// vi:noai:sw=4

#ifndef COMMON__SUPPORT__HXX
#define COMMON__SUPPORT__HXX

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define UNUSED(x) UNUSED_ ## x __attribute__((unused))

#define PRINT(output) \
    do { \
        std::cout \
            << __FILE__ << ":" << __LINE__ << " " \
            << output \
            << std::endl; \
    } while (false)

#define WARNING(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << output  \
            << std::endl; \
    } while (false)

#define ERROR(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << output  \
            << std::endl; \
    } while (false)

#define FATAL(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << output  \
            << std::endl; \
        std::terminate(); \
    } while (false)

#define NYI(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << "NYI: " << output  \
            << std::endl; \
    } while (false)

// ENFORCE (and its variants) never get compiled out.
#define ENFORCE(condition, output) \
    do { \
        if (!LIKELY(condition)) { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << output  \
                << "  (("#condition"))" \
                << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define ENFORCE_SYS(condition, output) \
    ENFORCE(condition, output << " (" << ::strerror(errno) << ")")

// ASSERT (and its variants) may be compiled out.
#if 1
#  define ASSERT(condition, output) \
    do { \
        if (!LIKELY(condition)) { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << output  \
                << "  (("#condition"))" \
                << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#  define ASSERT_SYS(condition, output) \
    ASSERT(condition, output << " (" << ::strerror(errno) << ")")
#else
#  define ASSERT(condition, output) \
    do { } while (false)
#  define ASSERT_SYS(condition, output) \
    do { } while (false)
#endif

template <typename T> T clamp(T t, T min, T max) {
    if      (t < min) { return min; }
    else if (t > max) { return max; }
    else              { return t;   }
}

template <typename T> std::string stringify(const T & t) {
    std::ostringstream ost;
    ost << t;
    return ost.str();
}

template <typename T> T unstringify(const std::string & str) {
    std::istringstream ist(str + '\n');
    T t;
    ist >> t;
    if (ist.good()) { return t; }
    else { FATAL("Failed to convert: " << str); }   // FIXME use exception
}

// Explicit Sign
template <typename T> std::string expSignStr(T t) {
    std::ostringstream ost;
    if (t < 0) { ost <<  t; }
    else       { ost << '+' << t; }
    return ost.str();
}

template <typename T> std::string nthStr(T t) {
    std::ostringstream ost;
    ost << t;
    switch (t) {
        case 0:
            ost << "th";
            break;
        case 1:
            ost << "st";
            break;
        case 2:
            ost << "nd";
            break;
        case 3:
            ost << "rd";
            break;
        default:
            ost << "th";
            break;
    }
    return ost.str();
}

inline char nibbleToHex(uint8_t nibble) {
    ASSERT(nibble < 0x10, "");
    if (nibble < 0xA) return '0' +  nibble;
    else              return 'A' + (nibble - 10);
}

template <typename T> std::string toHex(T t) {
    std::string str;
    for (size_t i = 0; i < sizeof(T); ++i) {
        str.push_back(nibbleToHex(t >> (8 * (sizeof(T) - i - 1) + 4) & 0x0F));
        str.push_back(nibbleToHex(t >> (8 * (sizeof(T) - i - 1))     & 0x0F));
    }
    return str;
}

template <typename T> std::string toBinary(T t) {
    std::string str;
    for (size_t i = 0; i < 8 * sizeof(T); ++i) {
        str.push_back(((t >> (8 * sizeof(T) - i - 1)) & 0x1) ? '1' : '0');
    }
    return str;
}

// Inherit from this to be uncopyable.
class Uncopyable {
    Uncopyable(const Uncopyable &) {}
    Uncopyable & operator = (const Uncopyable &) { return *this; }

public:
    Uncopyable() {}
};

class Timer {
    uint32_t _sec;
    uint32_t _usec;

public:
    Timer(uint32_t milliseconds);
    bool expired() const;
};

enum class Esc {
    RESET,

    BOLD,
    DIM,
    UNDERLINE,
    BLINK,
    REVERSE,
    HIDDEN,

    CL_BOLD,
    CL_UNDERLINE,
    CL_BLINK,
    CL_REVERSE,
    CL_COLOR,

    FG_BLACK,
    FG_RED,
    FG_GREEN,
    FG_YELLOW,
    FG_BLUE,
    FG_MAGENTA,
    FG_CYAN,
    FG_WHITE,

    BG_BLACK,
    BG_RED,
    BG_GREEN,
    BG_YELLOW,
    BG_BLUE,
    BG_MAGENTA,
    BG_CYAN,
    BG_WHITE
};

std::ostream & operator << (std::ostream & ost, Esc e);

#endif // COMMON__SUPPORT__HXX
