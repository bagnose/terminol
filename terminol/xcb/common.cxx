// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#include "terminol/xcb/common.hxx"

#include <iostream>
#include <sstream>
#include <cstdlib>

bool _xcb_request_failed(xcb_connection_t  * connection,
                         xcb_void_cookie_t   cookie,
                         const char        * err_msg,
                         const char        * filename,
                         int                 line) {
    auto error = xcb_request_check(connection, cookie);
    if (error) {
        std::cerr
            << filename << ':' << line << ' ' << err_msg
            << " Bad " << stringifyError(error) << '.'
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

std::string stringifyError(xcb_generic_error_t * error) {
    std::ostringstream ost;

#if 0
     typedef struct {
    uint8_t   response_type;  /**< Type of the response */
    uint8_t   error_code;     /**< Error code */
    uint16_t sequence;       /**< Sequence number */
    uint32_t resource_id;     /** < Resource ID for requests with side effects only */
    uint16_t minor_code;      /** < Minor opcode of the failed request */
    uint8_t major_code;       /** < Major opcode of the failed request */
    uint8_t pad0;
    uint32_t pad[5];         /**< Padding */
    uint32_t full_sequence;  /**< full sequence */
} xcb_generic_error_t;
#endif

    switch (error->error_code) {
        case XCB_REQUEST:
        {
            // xcb_request_error_t
            //auto e = reinterpret_cast<xcb_request_error_t *>(error);
            ost << "request";
            break;
        }
        case XCB_VALUE:
        {
            // xcb_value_error_t
            ost << "value";
            break;
        }
        case XCB_WINDOW:
        {
            // xcb_value_error_t xcb_window_error_t
            ost << "window";
            break;
        }
        case XCB_PIXMAP:
        {
            // xcb_value_error_t xcb_pixmap_error_t
            ost << "pixmap";
            break;
        }
        case XCB_ATOM:
        {
            // xcb_value_error_t xcb_atom_error_t
            ost << "atom";
            break;
        }
        case XCB_CURSOR:
        {
            // xcb_value_error_t xcb_cursor_error_t
            ost << "cursor";
            break;
        }
        case XCB_FONT:
        {
            // xcb_value_error_t xcb_font_error_t
            ost << "font";
            break;
        }
        case XCB_MATCH:
        {
            // xcb_request_error_t xcb_match_error_t
            ost << "match";
            break;
        }
        case XCB_DRAWABLE:
        {
            // xcb_value_error_t xcb_drawable_error_t
            ost << "drawable";
            break;
        }
        case XCB_ACCESS:
        {
            // xcb_request_error_t xcb_access_error_t
            ost << "access";
            break;
        }
        case XCB_ALLOC:
        {
            // xcb_request_error_t xcb_alloc_error_t
            ost << "alloc";
            break;
        }
        case XCB_COLORMAP:
        {
            // xcb_value_error_t xcb_colormap_error_t
            ost << "colormap";
            break;
        }
        case XCB_G_CONTEXT:
        {
            // xcb_value_error_t xcb_g_context_error_t
            ost << "graphical-context";
            break;
        }
        case XCB_ID_CHOICE:
        {
            // xcb_value_error_t xcb_id_choice_error_t
            ost << "id-choice";
            break;
        }
        case XCB_NAME:
        {
            // xcb_request_error_t xcb_name_error_t
            ost << "name";
            break;
        }
        case XCB_LENGTH:
        {
            // xcb_request_error_t xcb_length_error_t
            ost << "length";
            break;
        }
        case XCB_IMPLEMENTATION:
        {
            // xcb_request_error_t xcb_implementation_error_t
            ost << "implementation";
            break;
        }
        default:
        {
            ost << "<unknown>";
            break;
        }
    }

    return ost.str();
}
