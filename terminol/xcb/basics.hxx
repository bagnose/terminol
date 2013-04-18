// vi:noai:sw=4

#ifndef BASICS__HXX
#define BASICS__HXX

#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

class X_Basics : protected Uncopyable {
    xcb_connection_t  * _connection;
    xcb_screen_t      * _screen;
    xcb_visualtype_t  * _visual;
    xcb_key_symbols_t * _keySymbols;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    X_Basics() throw (Error) {
        int screenNum;
        _connection = xcb_connect(nullptr, &screenNum);
        if (xcb_connection_has_error(_connection)) {
            throw Error("Couldn't open display.");
        }

        const xcb_setup_t * setup = xcb_get_setup(_connection);
        xcb_screen_iterator_t screenIter = xcb_setup_roots_iterator(setup);
        for (int i = 0; i != screenNum; ++i) {
            xcb_screen_next(&screenIter);
        }
        _screen = screenIter.data;

        _visual = nullptr;
        for (xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(_screen);
             depth_iter.rem;
             xcb_depth_next(&depth_iter))
        {
            xcb_visualtype_iterator_t visual_iter;

            for (visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
                 visual_iter.rem;
                 xcb_visualtype_next(&visual_iter))
            {
                if (_screen->root_visual == visual_iter.data->visual_id) {
                    _visual = visual_iter.data;
                    break;
                }
            }
        }
        ENFORCE(_visual, "");

        _keySymbols = xcb_key_symbols_alloc(_connection);
    }

    ~X_Basics() {
        xcb_key_symbols_free(_keySymbols);
        xcb_disconnect(_connection);
    }

    xcb_connection_t  * connection() { return _connection; }
    xcb_screen_t      * screen()     { return _screen;     }
    xcb_visualtype_t  * visual()     { return _visual;     }
    xcb_key_symbols_t * keySymbols() { return _keySymbols; }
};

#endif // BASICS__HXX
