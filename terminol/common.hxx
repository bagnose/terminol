// vi:noai:sw=4

#ifndef COMMON__HXX
#define COMMON__HXX

#include <iostream>
#include <sstream>
#include <cstring>

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define PRINT(output) \
    do { \
        std::cout \
            << __FILE__ << ":" << __LINE__ << " " \
            << "" output \
            << std::endl; \
    } while (false)

#define WARNING(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << "" output  \
            << std::endl; \
    } while (false)

#define ERROR(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << "" output  \
            << std::endl; \
    } while (false)

#define FATAL(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << "" output  \
            << std::endl; \
        std::terminate(); \
    } while (false)

// ENFORCE (and its variants) never get compiled out.
#define ENFORCE(condition, output) \
    do { \
        if (!LIKELY(condition)) { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << "" output  \
                << "  (("#condition"))" \
                << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#define ENFORCE_SYS(condition, output) \
    ENFORCE(condition, "" output << " (" << ::strerror(errno) << ")")

// ASSERT (and its variants) may be compiled out.
#if 1
#  define ASSERT(condition, output) \
    do { \
        if (!LIKELY(condition)) { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << "" output  \
                << "  (("#condition"))" \
                << std::endl; \
            std::terminate(); \
        } \
    } while (false)

#  define ASSERT_SYS(condition, output) \
    ASSERT(condition, "" output << " (" << ::strerror(errno) << ")")
#else
#  define ASSERT(condition, output) \
    do { } while (false)
#  define ASSERT_SYS(condition, output) \
    do { } while (false)
#endif

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
    else { FATAL("Failed to convert: " << str); }
}

// Inherit from this to be uncopyable.
class Uncopyable {
public:
    Uncopyable() {}

private:
    Uncopyable(const Uncopyable &) {}
    Uncopyable & operator = (const Uncopyable &) { return *this; }
};

#endif // COMMON__HXX
