// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__DEBUG__HXX
#define SUPPORT__DEBUG__HXX

#include <iostream>
#include <cstring>
#include <cerrno>

using TerminateHandler = void (*)();

[[noreturn]] void terminate();
TerminateHandler  setTerminate(TerminateHandler f) noexcept;
TerminateHandler  getTerminate() noexcept;

#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)

#define UNUSED(x) UNUSED_##x __attribute__((unused))

// BSD doesn't have EINTR
#ifndef __linux__
#define TEMP_FAILURE_RETRY(a) (a)
#endif

struct DebugDummyInserter {};
inline std::ostream & operator<<(std::ostream & ost, const DebugDummyInserter &) {
    return ost;
}

#define PRINT(output) \
    do { std::cout << __FILE__ << ":" << __LINE__ << " " output << std::endl; } while (false)

#define WARNING(output) \
    do { std::cerr << __FILE__ << ":" << __LINE__ << " " output << std::endl; } while (false)

#define ERROR(output) \
    do { std::cerr << __FILE__ << ":" << __LINE__ << " " output << std::endl; } while (false)

#define FATAL(output)                                                                  \
    do {                                                                               \
        std::cerr << __FILE__ << ":" << __LINE__ << " " << DebugDummyInserter() output \
                  << std::endl;                                                        \
        terminate();                                                                   \
    } while (false)

// Perform the action exactly once
#define DO_ONCE(action)                 \
    do {                                \
        static bool done_DEBUG = false; \
        if (!done_DEBUG) {              \
            done_DEBUG = true;          \
            action;                     \
        }                               \
    } while (false)

#if DEBUG
#define NYI(output)                                                       \
    do {                                                                  \
        std::cerr << __FILE__ << ":" << __LINE__ << " "                   \
                  << "NYI: " << DebugDummyInserter() output << std::endl; \
    } while (false)
#else
#define NYI(output)                                                       \
    DO_ONCE(do {                                                          \
        std::cerr << __FILE__ << ":" << __LINE__ << " "                   \
                  << "NYI: " << DebugDummyInserter() output << std::endl; \
    } while (false))
#endif

// ENFORCE never gets compiled out
#define ENFORCE(condition, output)                                                         \
    do {                                                                                   \
        if (!LIKELY(condition)) {                                                          \
            std::cerr << __FILE__ << ":" << __LINE__ << " " << DebugDummyInserter() output \
                      << "  ((" #condition "))" << std::endl;                              \
            terminate();                                                                   \
        }                                                                                  \
    } while (false)

// ASSERT may be compiled out
#if DEBUG
#define ASSERT(condition, output)                                                          \
    do {                                                                                   \
        if (!LIKELY(condition)) {                                                          \
            std::cerr << __FILE__ << ":" << __LINE__ << " " << DebugDummyInserter() output \
                      << "  ((" #condition "))" << std::endl;                              \
            terminate();                                                                   \
        }                                                                                  \
    } while (false)
#else
#define ASSERT(condition, output) \
    do {                          \
    } while (false)
#endif

#endif // SUPPORT__DEBUG__HXX
