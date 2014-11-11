// vi:noai:sw=4
// Copyright © 2014 David Bryant

#include "terminol/xcb/widget.hxx"

Widget::Widget(I_Dispatcher & dispatcher,
               Basics       & basics,
               uint32_t       background,
               int16_t x, int16_t y, uint16_t width, uint16_t height) :
    _dispatcher(dispatcher),
    _basics(basics)
{
    xcb_void_cookie_t cookie;

    // Create the window.

    uint32_t winValues[] = {
        // XCB_CW_BACK_PIXEL
        // Note, it is important to set XCB_CW_BACK_PIXEL to the actual
        // background colour used by the terminal in order to prevent
        // flicker when the window is exposed.
        background,
        // XCB_CW_BIT_GRAVITY
        XCB_GRAVITY_NORTH_WEST,         // What to do if window is resized.
        // XCB_CW_WIN_GRAVITY
        XCB_GRAVITY_NORTH_WEST,         // What to do if parent is resized.
        // XCB_CW_BACKING_STORE
        XCB_BACKING_STORE_NOT_USEFUL,   // XCB_BACKING_STORE_WHEN_MAPPED, XCB_BACKING_STORE_ALWAYS
        // XCB_CW_SAVE_UNDER
        0,                              // 1 -> useful
        // XCB_CW_EVENT_MASK
        XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
            XCB_EVENT_MASK_POINTER_MOTION_HINT | XCB_EVENT_MASK_POINTER_MOTION |
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            XCB_EVENT_MASK_FOCUS_CHANGE,
        // XCB_CW_CURSOR
        _basics.normalCursor()
    };

    uint32_t winMask =
        XCB_CW_BACK_PIXEL    |
        XCB_CW_BIT_GRAVITY   |
        XCB_CW_WIN_GRAVITY   |
        XCB_CW_BACKING_STORE |
        XCB_CW_SAVE_UNDER    |
        XCB_CW_EVENT_MASK    |
        XCB_CW_CURSOR
        ;

    _window = xcb_generate_id(_basics.connection());
    cookie  = xcb_create_window_checked(_basics.connection(),
                                        _basics.screen()->root_depth,
                                        _window,
                                        _basics.screen()->root,
                                        x, y,
                                        width, height,
                                        0,            // border width
                                        XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                        _basics.screen()->root_visual,
                                        winMask,
                                        winValues);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to create window.")) {
        throw Error("Failed to create window.");
    }
    auto windowGuard = scopeGuard([&] { xcb_destroy_window(_basics.connection(), _window); });

    // Flush our XCB calls.

    xcb_flush(_basics.connection());

    //gcGuard.dismiss();
    windowGuard.dismiss();

    _dispatcher.add(_window, this);
}

Widget::~Widget() {
    _dispatcher.remove(_window);
}

void Widget::resize(const xcb_rectangle_t & geometry) {
    const uint32_t values[] = {
        static_cast<uint32_t>(geometry.x),
        static_cast<uint32_t>(geometry.y),
        static_cast<uint32_t>(geometry.width),
        static_cast<uint32_t>(geometry.height)
    };

    auto cookie = xcb_configure_window_checked(_basics.connection(),
                                               _window,
                                               XCB_CONFIG_WINDOW_X     |
                                               XCB_CONFIG_WINDOW_Y     |
                                               XCB_CONFIG_WINDOW_WIDTH |
                                               XCB_CONFIG_WINDOW_HEIGHT,
                                               values);
    xcb_request_failed(_basics.connection(), cookie, "Failed to set geometry.");
}

void Widget::map() {
    auto cookie = xcb_map_window_checked(_basics.connection(), _window);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to map window")) {
        throw Error("Failed to map window.");
    }
}
