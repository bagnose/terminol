// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef XCB__COMMON__HXX
#define XCB__COMMON__HXX

#include <xcb/xcb.h>

#include <iostream>
#include <cstdlib>

#define xcb_request_failed(connection, cookie, err_msg) _xcb_request_failed(connection, cookie, err_msg, __LINE__)
namespace {

bool _xcb_request_failed(xcb_connection_t  * connection,
                         xcb_void_cookie_t   cookie,
                         const char        * err_msg,
                         int                 line) {
    auto error = xcb_request_check(connection, cookie);
    if (error) {
        std::cerr
            << __FILE__ << ':' << line << ' ' << err_msg
            << " (X Error Code: " << static_cast<int>(error->error_code) << ')'
            << std::endl;
        std::free(error);
        return true;
    }
    else {
        return false;
    }
}

std::string stringifyEventType(uint8_t responseType) {
    switch (responseType) {
        case XCB_KEY_PRESS:
            return "key-press";
        case XCB_KEY_RELEASE:
            return "key-release";
        case XCB_BUTTON_PRESS:
            return "button-press";
        case XCB_BUTTON_RELEASE:
            return "button-release";
        case XCB_MOTION_NOTIFY:
            return "motion-notify";
        case XCB_EXPOSE:
            return "expose";
        case XCB_ENTER_NOTIFY:
            return "enter";
        case XCB_LEAVE_NOTIFY:
            return "leave";
        case XCB_FOCUS_IN:
            return "focus-in";
        case XCB_FOCUS_OUT:
            return "focus-out";
        case XCB_MAP_NOTIFY:
            return "map-notify";
        case XCB_UNMAP_NOTIFY:
            return "unmap-notify";
        case XCB_CONFIGURE_NOTIFY:
            return "configure-notify";
        case XCB_VISIBILITY_NOTIFY:
            return "visibility-notify";
        case XCB_DESTROY_NOTIFY:
            return "destroy-notify";
        case XCB_SELECTION_CLEAR:
            return "selection-clear";
        case XCB_SELECTION_NOTIFY:
            return "selection-notify";
        case XCB_SELECTION_REQUEST:
            return "selection-request";
        case XCB_CLIENT_MESSAGE:
            return "client-message";
        case XCB_REPARENT_NOTIFY:
            return "parent-notify";
        default:
            return "<unknown>";
    }
}

} // namespace {anonymous}

#endif // XCB__COMMON__HXX
