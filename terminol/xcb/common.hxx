// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#ifndef XCB__COMMON__HXX
#define XCB__COMMON__HXX

#include <xcb/xcb.h>

#include <string>

#define xcb_request_failed(connection, cookie, err_msg) _xcb_request_failed(connection, cookie, err_msg, __FILE__, __LINE__)

bool _xcb_request_failed(xcb_connection_t  * connection,
                         xcb_void_cookie_t   cookie,
                         const char        * err_msg,
                         const char        * filename,
                         int                 line);

std::string stringifyEventType(uint8_t responseType);

std::string stringifyError(xcb_generic_error_t * error);

#endif // XCB__COMMON__HXX
