#ifndef SUPPORT__EXCEPTION__HXX
#define SUPPORT__EXCEPTION__HXX

#include <system_error>
#include <string>

// Base class for all terminol exceptions
class Exception : public std::runtime_error {
public:
    static void set_thread_location(const char * file, int line);
    static std::string get_thread_location();

protected:
    explicit Exception(const std::string & what_arg) :
        std::runtime_error(init(what_arg))
    {}

private:
    static std::string init(std::string what_arg) {
        auto loc = get_thread_location();
        if (!loc.empty()) {
            if (what_arg.empty()) {
                what_arg = loc;
            }
            else {
                what_arg += ": " + loc;
            }
        }
        return what_arg;
    }
};

// Generic error
class GenericError final : public Exception {
public:
    explicit GenericError(const std::string & what_arg) : Exception(what_arg) {}
};

// User error
class UserError final : public Exception {
    std::string _message;

public:
    explicit UserError(const std::string & message) :
        Exception(message),
        _message(message)
    {}

    const std::string & message() const { return _message; }
};

// Conversion error
class ConversionError final : public Exception {
public:
    explicit ConversionError(const std::string & what_arg) : Exception(what_arg) {}
};

// Terminol reimplementation of std::system_error
class SystemError final : public Exception {
    static std::string init(const std::error_code & ec, std::string what_arg) {
        if (ec) {
            if (!what_arg.empty()) {
                what_arg += ": ";
            }
            what_arg += ec.message();
        }
        return what_arg;
    }

    std::error_code _ec;

public:
    SystemError(std::error_code ec, const std::string & what_arg) :
        Exception(init(ec, what_arg)),
        _ec(ec)
    {}

    const std::error_code & code() const { return _ec; }
};

// Throw an exception, e.g.:
//
//     THROW(GenericError, "Something failed");
#define THROW(exception_) \
    do { \
        Exception::set_thread_location(__FILE__, __LINE__); \
        static_assert(std::is_base_of_v<std::exception, decltype(exception_)>); \
        throw exception_; \
    } while (false)

// Throw a SystemError because a system call failed (and THROW_IF_SYSCALL_FAILS()
// wasn't appropriate because certain failure modes were not considered exceptional).
//
//     // 'fd' has non-blocking behaviour
//     int rc = TEMP_FAILURE_RETRY(::connect(fd, ...));
//     if (rc == -1) {
//         switch (errno) {
//             case EINPROGRESS:
//                 // as expected for non-blocking connect
//                 break;
//             default:
//                 THROW_SYSTEM_ERROR(errno, "connect()"));
//         }
//     }
#define THROW_SYSTEM_ERROR(errno_, text) \
    do { \
        int errno_copy = errno_; \
        THROW(SystemError(std::error_code(errno_copy, std::generic_category()), text)); \
    } while (false)

// Throw an exception if a condition is not met, e.g.:
//
//     THROW_UNLESS(iter != container.end(), GenericError("Could not find entry"));
#define THROW_UNLESS(condition, exception) \
    do { \
        if (!(condition)) { \
            THROW(exception); \
        } \
    } while (false)

// Throw an exception if the system call fails (i.e. returns -1), e.g.:
//
// int fd = THROW_IF_SYSCALL_FAILS(::open("/path/to/file", ...), "open()");
#define THROW_IF_SYSCALL_FAILS(syscall, text) \
    (__extension__ \
        ({ long int rval_ = TEMP_FAILURE_RETRY(syscall); \
           if (rval_ == -1) { \
               THROW_SYSTEM_ERROR(errno, text); \
           } \
           rval_; }))

#endif // SUPPORT__EXCEPTION__HXX
