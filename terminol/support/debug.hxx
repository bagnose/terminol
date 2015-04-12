// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__DEBUG__HXX
#define SUPPORT__DEBUG__HXX

#include <iostream>
#include <cstring>
#include <cerrno>

typedef void (*TerminateHandler)();

[[noreturn]] void terminate();
TerminateHandler setTerminate(TerminateHandler f);
TerminateHandler getTerminate();

#define LIKELY(x)   __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define UNUSED(x) UNUSED_ ## x __attribute__((unused))

// BSD doesn't have EINTR
#ifndef __linux__
#define TEMP_FAILURE_RETRY(a) (a)
#endif

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
        terminate(); \
    } while (false)

// Perform the action exactly once
#define DO_ONCE(action) \
    do { \
        static bool done_DEBUG = false; \
        if (!done_DEBUG) { \
            done_DEBUG = true; \
            action; \
        } \
    } while (false)

#if DEBUG
#  define NYI(output) \
    do { \
        std::cerr \
            << __FILE__ << ":" << __LINE__ << " " \
            << "NYI: " << output  \
            << std::endl; \
    } while (false)
#else
#  define NYI(output) \
    DO_ONCE( \
        do { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << "NYI: " << output  \
                << std::endl; \
        } while (false) \
    )
#endif

// ENFORCE (and its variants) never get compiled out.
#define ENFORCE(condition, output) \
    do { \
        if (!LIKELY(condition)) { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << output  \
                << "  (("#condition"))" \
                << std::endl; \
            terminate(); \
        } \
    } while (false)

#define ENFORCE_SYS(condition, output) \
    ENFORCE(condition, output << " (" << ::strerror(errno) << ")")

// ASSERT (and its variants) may be compiled out.
#if DEBUG
#  define ASSERT(condition, output) \
    do { \
        if (!LIKELY(condition)) { \
            std::cerr \
                << __FILE__ << ":" << __LINE__ << " " \
                << output  \
                << "  (("#condition"))" \
                << std::endl; \
            terminate(); \
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

#endif // SUPPORT__DEBUG__HXX
