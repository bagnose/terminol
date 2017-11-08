// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#ifndef XCB__COMMON__HXX
#define XCB__COMMON__HXX

#include "terminol/support/conv.hxx"

#include <xcb/xcb.h>

#include <string>

#define THROW_IF_XCB_REQUEST_FAILED(connection, cookie, message)           \
    do {                                                                   \
        auto error = xcb_request_check(connection, cookie);                \
        if (error) {                                                       \
            ScopeGuard errorGuard([error]() { std::free(error); });        \
            THROW(XError(stringify(message, " ", stringifyError(error)))); \
        }                                                                  \
    } while (false)

#define CHECK_XCB_REQUEST(connection, cookie, message)              \
    (__extension__({                                                \
        bool rval_ = true;                                          \
        auto error = xcb_request_check(connection, cookie);         \
        if (error) {                                                \
            ScopeGuard errorGuard([error]() { std::free(error); }); \
            ERROR(message << " " << stringifyError(error));         \
            rval_ = false;                                          \
        }                                                           \
        rval_;                                                      \
    }))

std::string stringifyEventType(uint8_t responseType);

std::string stringifyError(xcb_generic_error_t * error);

#endif // XCB__COMMON__HXX
