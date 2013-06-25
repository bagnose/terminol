// vi:noai:sw=4

#include "terminol/xcb/window.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/pattern.hxx"

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_aux.h>
#include <pango/pangocairo.h>

#include <limits>

#include <unistd.h>

#define xcb_request_failed(connection, cookie, err_msg) _xcb_request_failed(connection, cookie, err_msg, __LINE__)
namespace {

bool _xcb_request_failed(xcb_connection_t * connection, xcb_void_cookie_t cookie,
                         const char * err_msg, int line) {
    auto error = xcb_request_check(connection, cookie);
    if (error) {
        std::cerr
            << '[' << __FILE__ << ':' << line << "] ERROR: " << err_msg
            << ". X Error Code: " << static_cast<int>(error->error_code)
            << std::endl;
        std::free(error);
        return true;
    }
    else {
        return false;
    }
}

} // namespace {anonymous}

Window::Window(I_Observer         & observer,
               const Config       & config,
               Deduper            & deduper,
               Basics             & basics,
               const ColorSet     & colorSet,
               FontSet            & fontSet,
               const KeyMap       & keyMap,
               const Tty::Command & command) throw (Error) :
    _observer(observer),
    _config(config),
    _basics(basics),
    _colorSet(colorSet),
    _fontSet(fontSet),
    _window(0),
    _destroyed(false),
    _gc(0),
    _width(0),
    _height(0),
    _tty(nullptr),
    _terminal(nullptr),
    _open(false),
    _pointerPos(Pos::invalid()),
    _mapped(false),
    _hadExpose(false),
    _pixmap(0),
    _surface(nullptr),
    _cr(nullptr),
    _title(_config.title),
    _icon(_config.icon),
    _primarySelection(),
    _clipboardSelection(),
    _pressed(false),
    _pressCount(0),
    _lastPressTime(0),
    _button(XCB_BUTTON_INDEX_ANY),
    _deferralsAllowed(true),
    _deferred(false)
{
    auto rows = _config.initialRows;
    auto cols = _config.initialCols;

    const auto BORDER_THICKNESS = _config.borderThickness;
    const auto SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    _width  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    _height = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();

    xcb_void_cookie_t cookie;

    //
    // Create the window.
    //

    uint32_t winValues[] = {
        // XCB_CW_BACK_PIXEL
        // Note, it is important to set XCB_CW_BACK_PIXEL to the actual
        // background colour used by the terminal in order to prevent
        // flicker when the window is exposed.
        _colorSet.getBackgroundPixel(),
        // XCB_CW_BIT_GRAVITY
        XCB_GRAVITY_NORTH_WEST,         // What to do if window is resized.
        // XCB_CW_WIN_GRAVITY
        XCB_GRAVITY_NORTH_WEST,         // What to do if parent is resized.
        // XCB_CW_BACKING_STORE
        XCB_BACKING_STORE_NOT_USEFUL,   // XCB_BACKING_STORE_WHEN_MAPPED, XCB_BACKING_STORE_ALWAYS
        // XCB_CW_SAVE_UNDER
        0,                              // 1 -> useful
        // XCB_CW_CURSOR
        //                                 TODO
        // XCB_CW_EVENT_MASK
        XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
        XCB_EVENT_MASK_POINTER_MOTION_HINT | XCB_EVENT_MASK_BUTTON_MOTION |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_FOCUS_CHANGE |
        0
    };

    _window = xcb_generate_id(_basics.connection());
    cookie = xcb_create_window_checked(_basics.connection(),
                                       _basics.screen()->root_depth,
                                       _window,
                                       _basics.screen()->root,
                                       _config.initialX, config.initialY,
                                       _width, _height,
                                       0,            // border width
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       _basics.screen()->root_visual,
                                       XCB_CW_BACK_PIXEL |
                                       XCB_CW_BIT_GRAVITY |
                                       XCB_CW_WIN_GRAVITY |
                                       XCB_CW_BACKING_STORE |
                                       XCB_CW_SAVE_UNDER |
                                       //XCB_CW_CURSOR |
                                       XCB_CW_EVENT_MASK,
                                       winValues);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to create window")) {
        throw Error("Failed to create window.");
    }

    auto windowGuard = scopeGuard([&] {xcb_destroy_window(_basics.connection(), _window);});

    //
    // Do the ICCC jive.
    //

    icccmConfigure();

    //
    // Create the GC.
    //

    uint32_t gcValues[] = {
        0 // no exposures
    };

    _gc = xcb_generate_id(_basics.connection());
    cookie = xcb_create_gc_checked(_basics.connection(),
                                   _gc,
                                   _window,
                                   XCB_GC_GRAPHICS_EXPOSURES,
                                   gcValues);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to allocate GC")) {
        throw Error("Failed to create GC.");
    }

    auto gcGuard = scopeGuard([&] { xcb_free_gc(_basics.connection(), _window); });

    //
    // Create the TTY and terminal.
    //

    _tty = new Tty(_config, rows, cols, stringify(_window), command);
    _terminal = new Terminal(*this, _config, deduper, rows, cols, keyMap, *_tty);
    _open = true;

    //
    // Update the window title and map the window.
    //

    updateTitle();

    cookie = xcb_map_window_checked(_basics.connection(), _window);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to map window")) {
        throw Error("Failed to map window.");
    }

    xcb_flush(_basics.connection());

    gcGuard.dismiss();
    windowGuard.dismiss();
}

Window::~Window() {
    if (_mapped) {
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");

        cairo_surface_finish(_surface);
        cairo_surface_destroy(_surface);

        auto cookie = xcb_free_pixmap_checked(_basics.connection(), _pixmap);
        xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap");
    }
    else {
        ASSERT(!_surface, "");
        ASSERT(!_pixmap, "");
    }

    // Unwind constructor.

    delete _tty;
    delete _terminal;

    xcb_free_gc(_basics.connection(), _gc);

    // The window may have been destroyed exogenously.
    if (!_destroyed) {
        auto cookie = xcb_destroy_window_checked(_basics.connection(), _window);
        xcb_request_failed(_basics.connection(), cookie, "Failed to destroy window");
    }

    xcb_flush(_basics.connection());
}

void Window::read() {
    ASSERT(_open, "");
    _terminal->read();
}

bool Window::needsFlush() const {
    ASSERT(_open, "");
    return _terminal->needsFlush();
}

void Window::flush() {
    ASSERT(_open, "");
    _terminal->flush();
}

// Events:

void Window::keyPress(xcb_key_press_event_t * event) {
    if (!_open) { return; }

    xcb_keysym_t keySym;
    ModifierSet  modifiers;

    if (_basics.getKeySym(event->detail, event->state, keySym, modifiers)) {
        _terminal->keyPress(keySym, modifiers);
    }
}

void Window::keyRelease(xcb_key_release_event_t * UNUSED(event)) {
    if (!_open) { return; }
}

void Window::buttonPress(xcb_button_press_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    //PRINT("Button-press: " << event->event_x << " " << event->event_y);
    if (!_open) { return; }
    if (event->detail < XCB_BUTTON_INDEX_1 ||
        event->detail > XCB_BUTTON_INDEX_5) { return; }

    auto modifiers = _basics.convertState(event->state);

    Pos  pos;
    auto within = xy2Pos(event->event_x, event->event_y, pos);

    switch (event->detail) {
        case XCB_BUTTON_INDEX_4:
            _terminal->scrollWheel(Terminal::ScrollDir::UP, modifiers, within, pos);
            return;
        case XCB_BUTTON_INDEX_5:
            _terminal->scrollWheel(Terminal::ScrollDir::DOWN, modifiers, within, pos);
            return;
    }

    if (_pressed) {
        ASSERT(event->detail != _button, "Already pressed!");
        return;
    }

    _pressed = true;

    if (_button != event->detail ||
        event->time - _lastPressTime > _config.doubleClickTimeout)
    {
        _pressCount = 1;
    }
    else {
        ++_pressCount;
    }

    _button        = event->detail;
    _lastPressTime = event->time;

    switch (event->detail) {
        case XCB_BUTTON_INDEX_1:
            _terminal->buttonPress(Terminal::Button::LEFT, _pressCount,
                                   modifiers, within, pos);
            return;
        case XCB_BUTTON_INDEX_2:
            _terminal->buttonPress(Terminal::Button::MIDDLE, _pressCount,
                                   modifiers, within, pos);
            return;
        case XCB_BUTTON_INDEX_3:
            _terminal->buttonPress(Terminal::Button::RIGHT, _pressCount,
                                   modifiers, within, pos);
            return;
    }
}

void Window::buttonRelease(xcb_button_release_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    //PRINT("Button-release: " << event->event_x << " " << event->event_y);
    if (!_open) { return; }
    if (event->detail < XCB_BUTTON_INDEX_1 ||
        event->detail > XCB_BUTTON_INDEX_5) { return; }

    auto modifiers = _basics.convertState(event->state);

    if (_pressed && _button == event->detail) {
        _terminal->buttonRelease(false, modifiers);
        _pressed = false;
    }
}

void Window::motionNotify(xcb_motion_notify_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    //PRINT("Motion-notify: " << event->event_x << " " << event->event_y);
    if (!_open) { return; }
    if (!_pressed) { return; }

    int16_t x, y;

    if (event->detail == XCB_MOTION_HINT) {
        auto cookie = xcb_query_pointer(_basics.connection(), _window);
        auto reply  = xcb_query_pointer_reply(_basics.connection(), cookie, nullptr);
        if (!reply) {
            WARNING("Failed to query pointer.");
            return;
        }
        x = reply->win_x;
        y = reply->win_y;
        std::free(reply);
    }
    else {
        x = event->event_x;
        y = event->event_y;
    }

    Pos  pos;
    auto within = xy2Pos(x, y, pos);

    if (_pointerPos != pos) {
        auto modifiers = _basics.convertState(event->state);

        _pointerPos = pos;
        _terminal->buttonMotion(modifiers, within, pos);
    }

}

void Window::mapNotify(xcb_map_notify_event_t * UNUSED(event)) {
    //PRINT("Map");
    ASSERT(!_mapped, "");

    _pixmap = xcb_generate_id(_basics.connection());
    // Note, we create the pixmap against the root window rather than
    // _window to avoid dealing with the case where _window may have been
    // asynchronously destroyed.
    auto cookie = xcb_create_pixmap_checked(_basics.connection(),
                                            _basics.screen()->root_depth,
                                            _pixmap,
                                            _basics.screen()->root,
                                            _width,
                                            _height);
    xcb_request_failed(_basics.connection(), cookie, "Failed to create pixmap");

    _surface = cairo_xcb_surface_create(_basics.connection(),
                                        _pixmap,
                                        _basics.visual(),
                                        _width,
                                        _height);
    ENFORCE(_surface, "Failed to create surface");
    ENFORCE(cairo_surface_status(_surface) == CAIRO_STATUS_SUCCESS, "");

    _mapped = true;
}

void Window::unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) {
    //PRINT("UnMap");
    ASSERT(_mapped, "");

    ASSERT(_surface, "");
    ENFORCE(cairo_surface_status(_surface) == CAIRO_STATUS_SUCCESS, "");
    cairo_surface_finish(_surface);
    cairo_surface_destroy(_surface);
    _surface = nullptr;

    ASSERT(_pixmap, "");
    auto cookie = xcb_free_pixmap(_basics.connection(), _pixmap);
    xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap");
    _pixmap = 0;

    _mapped = false;
}

void Window::reparentNotify(xcb_reparent_notify_event_t * UNUSED(event)) {
    //PRINT("Reparent");
}

void Window::expose(xcb_expose_event_t * event) {
    if (_deferred) { return; }

    ASSERT(event->window == _window, "Which window?");
    /*
    PRINT("Expose: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);
          */

    if (_mapped) {
        if (_hadExpose) {
            // Once we've had our first expose the pixmap is always valid.
            ASSERT(_pixmap, "");
            auto cookie = xcb_copy_area_checked(_basics.connection(),
                                                _pixmap,
                                                _window,
                                                _gc,
                                                event->x, event->y,
                                                event->x, event->y,
                                                event->width, event->height);
            xcb_request_failed(_basics.connection(), cookie, "Failed to copy area");
            xcb_flush(_basics.connection());
        }
        else {
            ASSERT(_surface, "");
#if 0
            ASSERT(event->x == 0 && event->y == 0, "");
            ASSERT(event->width == _width && event->height == _height, "");
            draw(event->x, event->y, event->width, event->height);
#else
            draw(0, 0, _width, _height);
#endif
        }
    }

    _hadExpose = true;
}

void Window::configureNotify(xcb_configure_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");

    // We are only interested in size changes (not moves).
    if (_width == event->width && _height == event->height) {
        return;
    }

    /*
    PRINT("Configure notify: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);
          */

    _width  = event->width;
    _height = event->height;

    if (_deferralsAllowed) {
        _observer.defer(this);
        _deferred = true;
    }
    else {
        deferral();
    }
}

void Window::deferral() {
    if (_mapped) {
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");

        cairo_surface_finish(_surface);
        cairo_surface_destroy(_surface);
        _surface = nullptr;

        auto cookie = xcb_free_pixmap_checked(_basics.connection(), _pixmap);
        xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap");
        _pixmap = 0;

        //
        //
        //

        _pixmap = xcb_generate_id(_basics.connection());
        // Note, we create the pixmap against the root window rather than
        // _window to avoid dealing with the case where _window may have been
        // asynchronously destroyed.
        cookie = xcb_create_pixmap_checked(_basics.connection(),
                                           _basics.screen()->root_depth,
                                           _pixmap,
                                           //_window,
                                           _basics.screen()->root,
                                           _width,
                                           _height);
        xcb_request_failed(_basics.connection(), cookie, "Failed to create pixmap");

        cairo_surface_finish(_surface);
        _surface = cairo_xcb_surface_create(_basics.connection(),
                                            _pixmap,
                                            _basics.visual(),
                                            _width,
                                            _height);
        ENFORCE(_surface, "Failed to create surface");
        ENFORCE(cairo_surface_status(_surface) == CAIRO_STATUS_SUCCESS, "");
    }

    uint16_t rows, cols;

    const auto BORDER_THICKNESS = _config.borderThickness;
    const auto SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    if (_width  > 2 * BORDER_THICKNESS + _fontSet.getWidth() + SCROLLBAR_WIDTH &&
        _height > 2 * BORDER_THICKNESS + _fontSet.getHeight())
    {
        uint16_t w = _width  - (2 * BORDER_THICKNESS + SCROLLBAR_WIDTH);
        uint16_t h = _height - (2 * BORDER_THICKNESS);

        rows = h / _fontSet.getHeight();
        cols = w / _fontSet.getWidth();
    }
    else {
        rows = cols = 1;
    }

    ASSERT(rows > 0 && cols > 0, "");

    if (_open) {
        _tty->resize(rows, cols);
    }

    _terminal->resize(rows, cols);      // Ok to resize if not open?

    updateTitle();

    if (_mapped) {
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");
        draw(0, 0, _width, _height);
    }

    _deferred = false;
}

void Window::focusIn(xcb_focus_in_event_t * UNUSED(event)) {
    _terminal->focusChange(true);
}

void Window::focusOut(xcb_focus_out_event_t * UNUSED(event)) {
    _terminal->focusChange(false);

}

void Window::enterNotify(xcb_enter_notify_event_t * UNUSED(event)) {
    //PRINT("enter");
}

void Window::leaveNotify(xcb_leave_notify_event_t * event) {
    //PRINT("leave: " << int(event->mode));

    // XXX total guess that this is how we ensure we release
    // the button...
    if (event->mode == 2) {
        if (_pressed) {
            _terminal->buttonRelease(true, ModifierSet());
            _pressed = false;
        }
    }
}

void Window::visibilityNotify(xcb_visibility_notify_event_t * UNUSED(event)) {
}

void Window::destroyNotify(xcb_destroy_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    //PRINT("Destroy notify");

    _tty->close();
    _open      = false;
    _destroyed = true;      // XXX why not just zero _window
}

void Window::selectionClear(xcb_selection_clear_event_t * UNUSED(event)) {
    //PRINT("Selection clear");

    _terminal->clearSelection();
}

void Window::selectionNotify(xcb_selection_notify_event_t * UNUSED(event)) {
    //PRINT("Selection notify");
    if (_open) {
        uint32_t offset = 0;        // 32-bit quantities

        for (;;) {
            auto cookie = xcb_get_property(_basics.connection(),
                                           false,     // delete
                                           _window,
                                           XCB_ATOM_PRIMARY,
                                           XCB_GET_PROPERTY_TYPE_ANY,
                                           offset,
                                           8192 / 4);

            auto reply = xcb_get_property_reply(_basics.connection(), cookie, nullptr);
            if (!reply) { break; }
            auto guard = scopeGuard([reply] { std::free(reply); });

            void * value  = xcb_get_property_value(reply);
            int    length = xcb_get_property_value_length(reply);
            if (length == 0) { break; }

            _terminal->paste(reinterpret_cast<const uint8_t *>(value), length);
            offset += (length + 3) / 4;
        }
    }
}

void Window::selectionRequest(xcb_selection_request_event_t * event) {
    ASSERT(event->owner == _window, "Which window?");

    xcb_selection_notify_event_t response;
    response.response_type = XCB_SELECTION_NOTIFY;
    response.time          = event->time;
    response.requestor     = event->requestor;
    response.selection     = event->selection;
    response.target        = event->target;
    response.property      = XCB_ATOM_NONE;        // reject by default

    if (event->target == _basics.atomTargets()) {
        xcb_atom_t atomUtf8String = _basics.atomUtf8String();
        auto cookie = xcb_change_property_checked(_basics.connection(),
                                                  XCB_PROP_MODE_REPLACE,
                                                  event->requestor,
                                                  event->property,
                                                  XCB_ATOM_ATOM,
                                                  32,
                                                  1,
                                                  &atomUtf8String);
        xcb_request_failed(_basics.connection(), cookie, "Failed to change property");
        response.property = event->property;
    }
    else if (event->target == _basics.atomUtf8String()) {
        std::string text;

        if (event->selection == _basics.atomPrimary()) {
            text = _primarySelection;
        }
        else if (event->selection == _basics.atomClipboard()) {
            text = _clipboardSelection;
        }
        else {
            ERROR("Unexpected selection");
        }

        auto cookie = xcb_change_property_checked(_basics.connection(),
                                                  XCB_PROP_MODE_REPLACE,
                                                  event->requestor,
                                                  event->property,
                                                  event->target,
                                                  8,
                                                  text.length(),
                                                  text.data());
        xcb_request_failed(_basics.connection(), cookie, "Failed to change property");
        response.property = event->property;
    }

    auto cookie = xcb_send_event_checked(_basics.connection(),
                                         true,
                                         event->requestor,
                                         0,
                                         reinterpret_cast<const char *>(&response));
    xcb_request_failed(_basics.connection(), cookie, "Failed to send event");

    xcb_flush(_basics.connection());        // Required?
}

void Window::icccmConfigure() {
    //
    // machine
    //

    const auto & hostname = _basics.hostname();
    if (!hostname.empty()) {
        xcb_icccm_set_wm_client_machine(_basics.connection(),
                                        _window,
                                        XCB_ATOM_STRING,
                                        8,
                                        hostname.size(),
                                        hostname.data());
    }

    //
    // class
    //

    std::string wm_class = "terminol";
    xcb_icccm_set_wm_class(_basics.connection(), _window,
                           wm_class.size(), wm_class.data());

    //
    // size
    //

    const auto BORDER_THICKNESS = _config.borderThickness;
    const auto SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    const auto BASE_WIDTH  = 2 * BORDER_THICKNESS + SCROLLBAR_WIDTH;
    const auto BASE_HEIGHT = 2 * BORDER_THICKNESS;

    const auto MIN_COLS = 8;
    const auto MIN_ROWS = 2;

    xcb_size_hints_t sizeHints;
    sizeHints.flags = 0;
    xcb_icccm_size_hints_set_min_size(&sizeHints,
                                      BASE_WIDTH  + MIN_COLS * _fontSet.getWidth(),
                                      BASE_HEIGHT + MIN_ROWS * _fontSet.getHeight());
    xcb_icccm_size_hints_set_base_size(&sizeHints,
                                       BASE_WIDTH,
                                       BASE_HEIGHT);
    xcb_icccm_size_hints_set_resize_inc(&sizeHints,
                                        _fontSet.getWidth(),
                                        _fontSet.getHeight());
    xcb_icccm_size_hints_set_win_gravity(&sizeHints, XCB_GRAVITY_NORTH_WEST);
    // XXX or call xcb_icccm_set_wm_normal_hints() ?
#if 0
    xcb_icccm_set_wm_size_hints(_basics.connection(),
                                _window,
                                XCB_ATOM_WM_NORMAL_HINTS,
                                &sizeHints);
#else
    xcb_icccm_set_wm_normal_hints(_basics.connection(),
                                  _window,
                                  &sizeHints);
#endif

    //
    // wm?
    //

    xcb_icccm_wm_hints_t wmHints;
    wmHints.flags = 0;
    xcb_icccm_wm_hints_set_input(&wmHints, 1 /* What value? */);
    //xcb_icccm_wm_hints_set_icon_pixmap
    xcb_icccm_set_wm_hints(_basics.connection(), _window, &wmHints);

    //
    // xcb_icccm_set_wm_protocols
    //

    // TODO
}

void Window::pos2XY(Pos pos, int & x, int & y) const {
    ASSERT(pos.row <= _terminal->getRows(), "");
    ASSERT(pos.col <= _terminal->getCols(), "");

    const auto BORDER_THICKNESS = _config.borderThickness;

    x = BORDER_THICKNESS + pos.col * _fontSet.getWidth();
    y = BORDER_THICKNESS + pos.row * _fontSet.getHeight();
}

bool Window::xy2Pos(int x, int y, Pos & pos) const {
    auto within = true;

    const int BORDER_THICKNESS = _config.borderThickness;

    // x / cols:

    if (x < BORDER_THICKNESS) {
        pos.col = 0;
        within = false;
    }
    else if (x < BORDER_THICKNESS + _fontSet.getWidth() * _terminal->getCols()) {
        pos.col = (x - BORDER_THICKNESS) / _fontSet.getWidth();
        ASSERT(pos.col < _terminal->getCols(),
               "col is: " << pos.col << ", getCols() is: " <<
               _terminal->getCols());
    }
    else {
        pos.col = _terminal->getCols();
        within = false;
    }

    // y / rows:

    if (y < BORDER_THICKNESS) {
        pos.row = 0;
        within = false;
    }
    else if (y < BORDER_THICKNESS + _fontSet.getHeight() * _terminal->getRows()) {
        pos.row = (y - BORDER_THICKNESS) / _fontSet.getHeight();
        ASSERT(pos.row < _terminal->getRows(),
               "row is: " << pos.row << ", getRows() is: " <<
               _terminal->getRows());
    }
    else {
        pos.row = _terminal->getRows();
        within = false;
    }

    return within;
}

void Window::updateTitle() {
    ASSERT(_terminal, "");

    std::ostringstream ost;

#if DEBUG
    ost << "<DEBUG> ";
#endif

    ost << "[" << _terminal->getCols() << 'x' << _terminal->getRows() << "] ";
    ost << _title;

    const auto & fullTitle = ost.str();

#if 1
    xcb_icccm_set_wm_name(_basics.connection(),
                          _window,
                          XCB_ATOM_STRING,
                          8,
                          fullTitle.size(),
                          fullTitle.data());
#else
    xcb_ewmh_set_wm_name(_basics.ewmhConnection(),
                         _window,
                         fullTitle.size(),
                         fullTitle.data());
#endif
}

void Window::updateIcon() {
    ASSERT(_terminal, "");

    std::ostringstream ost;

#if DEBUG
    ost << "<DEBUG> ";
#endif

    ost << "[" << _terminal->getCols() << 'x' << _terminal->getRows() << "] ";
    ost << _icon;

    const auto & fullIcon = ost.str();

#if 1
    xcb_icccm_set_wm_icon_name(_basics.connection(),
                               _window,
                               XCB_ATOM_STRING,
                               8,
                               fullIcon.size(),
                               fullIcon.data());
#else
    xcb_ewmh_set_wm_icon_name(_basics.ewmhConnection(),
                              _window,
                              fullIcon.size(),
                              fullIcon.data());
#endif
}

void Window::draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    ASSERT(_mapped, "");
    ASSERT(_pixmap, "");
    ASSERT(_surface, "");
    _cr = cairo_create(_surface);
    cairo_set_line_width(_cr, 1.0);

#if 0
    // Clear the damaged area so that we know we are completely drawing to it.

    xcb_rectangle_t rect = {
        static_cast<int16_t>(x), static_cast<int16_t>(y), w, h
    };

    xcb_poly_rectangle(_basics.connection(),
                       _pixmap,
                       _gc,
                       1,
                       &rect);
#endif

    cairo_save(_cr); {
        cairo_rectangle(_cr, x, y, w, h);       // implicit cast to double
        cairo_clip(_cr);

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));

        drawBorder();

        Pos posBegin;
        xy2Pos(x, y, posBegin);
        Pos posEnd;
        xy2Pos(x + w, y + h, posEnd);
        if (posEnd.col != _terminal->getCols()) { ++posEnd.col; }
        if (posEnd.row != _terminal->getRows()) { ++posEnd.row; }
        _terminal->redraw(posBegin, posEnd);

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));

    } cairo_restore(_cr);
    cairo_destroy(_cr);
    _cr = nullptr;

    cairo_surface_flush(_surface);      // Useful?
    ENFORCE(cairo_surface_status(_surface) == CAIRO_STATUS_SUCCESS, "");

    auto cookie = xcb_copy_area_checked(_basics.connection(),
                                        _pixmap,
                                        _window,
                                        _gc,
                                        x, y, // src
                                        x, y, // dst
                                        w, h);
    xcb_request_failed(_basics.connection(), cookie, "Failed to copy area");

    xcb_flush(_basics.connection());
}

void Window::drawBorder() {
    const auto BORDER_THICKNESS = _config.borderThickness;
    const auto SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    cairo_save(_cr); {
        const auto & bg = _colorSet.getBorderColor();
        cairo_set_source_rgb(_cr, bg.r, bg.g, bg.b);

        double x1 = BORDER_THICKNESS + _fontSet.getWidth() * _terminal->getCols();
        double x2 = _width - SCROLLBAR_WIDTH;

        double y1 = BORDER_THICKNESS + _fontSet.getHeight() * _terminal->getRows();
        double y2 = _height;

        // Left edge.
        cairo_rectangle(_cr,
                        0.0,
                        0.0,
                        static_cast<double>(BORDER_THICKNESS),
                        static_cast<double>(_height));
        cairo_fill(_cr);

        // Top edge.
        cairo_rectangle(_cr,
                        0.0,
                        0.0,
                        x2,
                        static_cast<double>(BORDER_THICKNESS));
        cairo_fill(_cr);

        // Right edge.
        cairo_rectangle(_cr,
                        x1,
                        0.0,
                        x2 - x1,
                        y2);
        cairo_fill(_cr);

        // Bottom edge.
        cairo_rectangle(_cr,
                        0.0,
                        y1,
                        x2,
                        y2 - y1);
        cairo_fill(_cr);
    } cairo_restore(_cr);
}

// Terminal::I_Observer implementation:

void Window::terminalGetDisplay(std::string & display) throw () {
    display = _basics.display();
}

void Window::terminalCopy(const std::string & text, bool clipboard) throw () {
    //PRINT("Copy: '" << text << "', clipboard: " << clipboard);

    xcb_atom_t atom;

    if (clipboard) {
        atom = _basics.atomClipboard();
        _clipboardSelection = text;
    }
    else {
        atom = _basics.atomPrimary();
        _primarySelection = text;
    }

    xcb_set_selection_owner(_basics.connection(), _window,
                            atom, XCB_CURRENT_TIME);
    xcb_flush(_basics.connection());
}

void Window::terminalPaste(bool clipboard) throw () {
    //PRINT("Copy clipboard: " << clipboard);

    xcb_atom_t atom = clipboard ? _basics.atomClipboard() : _basics.atomPrimary();

    xcb_convert_selection(_basics.connection(),
                          _window,
                          atom,
                          _basics.atomUtf8String(),
                          XCB_ATOM_PRIMARY, // property
                          XCB_CURRENT_TIME);

    xcb_flush(_basics.connection());
}

void Window::terminalResizeFont(int delta) throw () {
    PRINT("Resize font: " << delta);
}

void Window::terminalResetTitleAndIcon() throw () {
    _title = _config.title;
    _icon  = _config.icon;
    updateTitle();
    updateIcon();
}

void Window::terminalSetWindowTitle(const std::string & str) throw () {
    //PRINT("Set title: " << title);
    _title = str;
    updateTitle();
}

void Window::terminalSetIconName(const std::string & str) throw () {
    //PRINT("Set title: " << title);
    _icon = str;
    updateIcon();
}

void Window::terminalBeep() throw () {
    xcb_icccm_wm_hints_t wmHints;
    wmHints.flags = 0;
    xcb_icccm_wm_hints_set_urgency(&wmHints);
    xcb_icccm_set_wm_hints(_basics.connection(), _window, &wmHints);
}

void Window::terminalResizeBuffer(uint16_t rows, uint16_t cols) throw () {
    const auto BORDER_THICKNESS = _config.borderThickness;
    const auto SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    uint32_t width  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    uint32_t height = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();

    if (_width != width || _height != height) {
        uint32_t values[] = { width, height };
        auto cookie = xcb_configure_window(_basics.connection(),
                                           _window,
                                           XCB_CONFIG_WINDOW_WIDTH |
                                           XCB_CONFIG_WINDOW_HEIGHT,
                                           values);
        if (!xcb_request_failed(_basics.connection(), cookie,
                               "Failed to configure window")) {
            xcb_flush(_basics.connection());
            _deferralsAllowed = false;
            _observer.sync();
            _deferralsAllowed = true;
        }
    }
}

bool Window::terminalFixDamageBegin(bool internal) throw () {
    //PRINT("Damage begin, internal: " << std::boolalpha << internal);

    if (internal) {
        if (!_deferred && _mapped) {
            ASSERT(_mapped, "");
            ASSERT(_surface, "");
#if 0
            xcb_clear_area(_basics.connection(),
                           0,       // don't generate exposure event
                           _window,
                           0, 0, _width, _height);
#endif
            _cr = cairo_create(_surface);
            cairo_set_line_width(_cr, 1.0);
            return true;
        }
        else {
            return false;
        }
    }
    else {
        ASSERT(_mapped, "");
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");
        ASSERT(_cr, "");
        return true;
    }
}

void Window::terminalDrawBg(Pos    pos,
                            UColor color,
                            size_t count) throw () {
    ASSERT(_cr, "");

    cairo_save(_cr); {
        int x, y;
        pos2XY(pos, x, y);

        double w = count * _fontSet.getWidth();
        double h = _fontSet.getHeight();

        auto bg = getColor(color);
        cairo_set_source_rgb(_cr, bg.r, bg.g, bg.b);

        cairo_rectangle(_cr, x, y, w, h);
        cairo_fill(_cr);

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));
    } cairo_restore(_cr);
}

void Window::terminalDrawFg(Pos             pos,
                            UColor          color,
                            AttrSet         attrs,
                            const uint8_t * str,
                            size_t          size,
                            size_t          count) throw () {
    ASSERT(_cr, "");

    cairo_save(_cr); {
        auto layout = pango_cairo_create_layout(_cr);
        auto layoutGuard = scopeGuard([&] { g_object_unref(layout); });

        auto font = _fontSet.get(attrs.get(Attr::ITALIC), attrs.get(Attr::BOLD));
        pango_layout_set_font_description(layout, font);
        pango_layout_set_width(layout, -1);

        int x, y;
        pos2XY(pos, x, y);

        double w = count * _fontSet.getWidth();
        double h = _fontSet.getHeight();
        cairo_rectangle(_cr, x, y, w, h);
        cairo_clip(_cr);

        auto alpha = attrs.get(Attr::CONCEAL) ? 0.1 : attrs.get(Attr::FAINT) ? 0.5 : 1.0;
        auto fg    = getColor(color);
        cairo_set_source_rgba(_cr, fg.r, fg.g, fg.b, alpha);

        if (attrs.get(Attr::UNDERLINE)) {
            cairo_move_to(_cr, x, y + h - 0.5);
            cairo_rel_line_to(_cr, w, 0.0);
            cairo_stroke(_cr);
        }

        cairo_move_to(_cr, x, y);
        pango_layout_set_text(layout, reinterpret_cast<const char *>(str), size);
        pango_cairo_update_layout(_cr, layout);
        pango_cairo_show_layout(_cr, layout);

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));
    } cairo_restore(_cr);
}

void Window::terminalDrawCursor(Pos             pos,
                                UColor          fg_,
                                UColor          bg_,
                                AttrSet         attrs,
                                const uint8_t * str,
                                size_t          size,
                                bool            wrapNext,
                                bool            focused) throw () {
    ASSERT(_cr, "");

    cairo_save(_cr); {
        auto layout = pango_cairo_create_layout(_cr);
        auto font   = _fontSet.get(attrs.get(Attr::ITALIC), attrs.get(Attr::BOLD));
        pango_layout_set_font_description(layout, font);

        pango_layout_set_width(layout, -1);
        pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);

        auto fg =
            _config.customCursorTextColor ?
            _colorSet.getCursorTextColor() :
            getColor(bg_);

        auto bg =
            _config.customCursorFillColor ?
            _colorSet.getCursorFillColor() :
            getColor(fg_);

        int x, y;
        pos2XY(pos, x, y);

        if (!focused) {
            cairo_set_source_rgb(_cr, fg.r, fg.g, fg.b);
            cairo_rectangle(_cr, x, y, _fontSet.getWidth(), _fontSet.getHeight());
            cairo_fill(_cr);
        }

        auto alpha = wrapNext ? 0.4 : 1.0;
        cairo_set_source_rgba(_cr, bg.r, bg.g, bg.b, alpha);

        if (focused) {
            cairo_rectangle(_cr, x, y, _fontSet.getWidth(), _fontSet.getHeight());
            cairo_fill(_cr);
            cairo_set_source_rgb(_cr, fg.r, fg.g, fg.b);
        }
        else {
            cairo_rectangle(_cr,
                            x + 0.5, y + 0.5,
                            _fontSet.getWidth() - 1.0, _fontSet.getHeight() - 1.0);
            cairo_stroke(_cr);
        }

        cairo_move_to(_cr, x, y);
        pango_layout_set_text(layout, reinterpret_cast<const char *>(str), size);
        pango_cairo_update_layout(_cr, layout);
        pango_cairo_show_layout(_cr, layout);

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));

        /* Free resources */
        g_object_unref(layout);
    } cairo_restore(_cr);
}

namespace {

void pathSingleLineSelection(cairo_t * cr,
                             double x0, double y0, double x1, double y1,
                             double i, double c) {
    auto d = M_PI / 180.0;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x0 + i + c, y0 + i + c, c, 180 * d, 270 * d);
    cairo_line_to(cr, x1 - i, y0 + i);
    cairo_arc(cr, x1 - i - c, y1 - i - c, c, 0 * d, 90 * d);
    cairo_line_to(cr, x0 + i, y1 - i);
    cairo_close_path(cr);
}

void pathWrappedLineSelection(cairo_t * cr,
                              double x0, double x1, double x2, double x3,
                              double y0, double y1, double y2,
                              double i, double c,
                              bool first) {
    auto d = M_PI / 180.0;

    if (first) {
        cairo_new_sub_path(cr);
        cairo_arc(cr, x2 + i + c, y0 + i + c, c, 180 * d, 270 * d);
        cairo_line_to(cr, x3 - i, y0 + i);
        cairo_line_to(cr, x3 - i, y1 - i);
        cairo_line_to(cr, x2 + i, y1 - i);
        cairo_close_path(cr);
    }
    else {
        cairo_new_sub_path(cr);
        cairo_move_to(cr, x0 + i, y1 + i);
        cairo_line_to(cr, x1 - i, y1 + i);
        cairo_arc(cr, x1 - i - c, y2 - i - c, c, 0 * d, 90 * d);
        cairo_line_to(cr, x0 + i, y2 - i);
        cairo_close_path(cr);
    }
}

void pathMultiLineSelection(cairo_t * cr,
                            double x0, double x1, double x2, double x3,
                            double y0, double y1, double y2, double y3,
                            double i, double c) {
    auto d = M_PI / 180.0;

    cairo_new_sub_path(cr);
    cairo_arc(cr, x1 + i + c, y0 + i + c, c, 180 * d, 270 * d);
    cairo_line_to(cr, x3 - i, y0 + i);
    cairo_line_to(cr, x3 - i, y2 - i);
    cairo_line_to(cr, x2 - i, y2 - i);
    cairo_arc(cr, x2 - i - c, y3 - i - c, c, 0 * d, 90 * d);
    cairo_line_to(cr, x0 + i, y3 - i);
    cairo_line_to(cr, x0 + i, y1 + i);
    cairo_line_to(cr, x1 + i, y1 + i);
    cairo_close_path(cr);
}

} // namespace {anonymous}

void Window::terminalDrawSelection(Pos  begin,
                                   Pos  end,
                                   bool topless,
                                   bool bottomless) throw () {
    ASSERT(!topless    || begin.row == 0, "");
    ASSERT(!bottomless || end.row   == _terminal->getRows(), "");
    ASSERT(begin.row <= end.row, "");

    ASSERT(_cr, "");

    double lineWidth = 1.0;
    double halfWidth = lineWidth / 2.0;
    double curve = 4.0;

    auto numCols = _terminal->getCols();
    auto plus    = bottomless ? 0 : 1;

    double bg[4] = { 0.4, 0.0, 1.0, 0.07 };
    double fg[4] = { 0.0, 0.5, 1.0, 1.0 };

    if (begin.row == end.row) {
        ASSERT(!topless && !bottomless, "");
        ASSERT(begin.col < end.col, "");

        //       0                  1
        //
        //   0   0------------------1
        //   1   3------------------2

        int x0, y0;
        int x1, y1;
        pos2XY(begin,      x0, y0);
        pos2XY(end.down(), x1, y1);

        pathSingleLineSelection(_cr,
                                x0, y0, x1, y1,
                                halfWidth,
                                curve);

        cairo_set_source_rgba(_cr, bg[0], bg[1], bg[2], bg[3]);
        cairo_fill_preserve(_cr);

        cairo_set_source_rgba(_cr, fg[0], fg[1], fg[2], fg[3]);
        cairo_stroke(_cr);
    }
    else if (begin.row + 1 == end.row && begin.col >= end.col)
    {
        //       0   1   2                  3
        //
        //   0           0------------------1
        //   1   4---5   3------------------2
        //   2   7---6

        int x0, x1, x2, x3;
        int y0, y1, y2;

        pos2XY(Pos(begin.row,      0),         x0, y0);
        pos2XY(Pos(end.row,        end.col),   x1, y1);
        pos2XY(Pos(end.row + plus, begin.col), x2, y2);
        pos2XY(Pos(end.row + plus, numCols),   x3, y2);      // clobber y2 with same value

        if (topless)    { y0 = -1; }
        if (bottomless) { y2 = _height + 1; }

        pathWrappedLineSelection(_cr, x0, x1, x2, x3, y0, y1, y2, halfWidth, curve, true);

        cairo_set_source_rgba(_cr, bg[0], bg[1], bg[2], bg[3]);
        cairo_fill_preserve(_cr);

        cairo_set_source_rgba(_cr, fg[0], fg[1], fg[2], fg[3]);
        cairo_stroke(_cr);

        pathWrappedLineSelection(_cr, x0, x1, x2, x3, y0, y1, y2, halfWidth, curve, false);

        cairo_set_source_rgba(_cr, bg[0], bg[1], bg[2], bg[3]);
        cairo_fill_preserve(_cr);

        cairo_set_source_rgba(_cr, fg[0], fg[1], fg[2], fg[3]);
        cairo_stroke(_cr);
    }
    else {
        // There are 8 distinct points, but they are defined by
        // 8 coordinates (not 16).
        // The general shape is:
        //
        //       0       1      2           3
        //
        //   0           0------------------1
        //   1   6-------7                  |
        //       |                          |
        //   2   |              3-----------2
        //   3   5--------------4
        //
        // Points are:
        //
        //   #: x, y
        //   -------
        //   0: 1, 0
        //   1: 3, 0
        //   2: 3, 2
        //   3: 2, 2
        //   4: 2, 3
        //   5: 0, 3
        //   6: 0, 1
        //   7: 1, 1

        int x0, x1, x2, x3;
        int y0, y1, y2, y3;

        pos2XY(Pos(begin.row,      0),         x0, y0);  // top left
        pos2XY(Pos(begin.row + 1,  begin.col), x1, y1);  // #7
        pos2XY(Pos(end.row,        end.col),   x2, y2);  // #3
        pos2XY(Pos(end.row + plus, numCols),   x3, y3);  // bottom right

        if (topless)    { y0 = -1; }
        if (bottomless) { y3 = _height + 1; }

        pathMultiLineSelection(_cr, x0, x1, x2, x3, y0, y1, y2, y3, halfWidth, curve);

        cairo_set_source_rgba(_cr, bg[0], bg[1], bg[2], bg[3]);
        cairo_fill_preserve(_cr);

        cairo_set_source_rgba(_cr, fg[0], fg[1], fg[2], fg[3]);
        cairo_stroke(_cr);
    }
}

void Window::terminalDrawScrollbar(size_t   totalRows,
                                   size_t   historyOffset,
                                   uint16_t visibleRows) throw () {
    ASSERT(_cr, "");

    const int SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    double x = static_cast<double>(_width - SCROLLBAR_WIDTH);
    double y = 0.0;
    double h = static_cast<double>(_height);
    double w = static_cast<double>(SCROLLBAR_WIDTH);

    // Draw the gutter.

    const auto & bg = _colorSet.getScrollBarBgColor();
    cairo_set_source_rgb(_cr, bg.r, bg.g, bg.b);

    cairo_rectangle(_cr,
                    x,
                    y,
                    w,
                    h);
    cairo_fill(_cr);

    // Draw the bar.

    auto yBar = static_cast<double>(historyOffset) / static_cast<double>(totalRows) * h;
    auto hBar = static_cast<double>(visibleRows)   / static_cast<double>(totalRows) * h;

    const auto & fg = _colorSet.getScrollBarFgColor();
    cairo_set_source_rgb(_cr, fg.r, fg.g, fg.b);

    cairo_rectangle(_cr,
                    x + 1.0,
                    yBar,
                    w - 2.0,
                    hBar);
    cairo_fill(_cr);
}

void Window::terminalFixDamageEnd(bool internal,
                                  Pos  begin,
                                  Pos  end,
                                  bool scrollBar) throw () {
    ASSERT(_cr, "");

    if (internal) {
        cairo_destroy(_cr);
        _cr = nullptr;

        cairo_surface_flush(_surface);      // Useful?

        int x0, y0;
        pos2XY(begin, x0, y0);
        int x1, y1;
        pos2XY(end, x1, y1);

        if (scrollBar) {
            // Expand the region to include the scroll bar
            y0 = 0;
            x1 = _width;
            y1 = _height;
        }

        // Copy the buffer region
        auto cookie = xcb_copy_area_checked(_basics.connection(),
                                            _pixmap,
                                            _window,
                                            _gc,
                                            x0, y0,   // src
                                            x0, y0,   // dst
                                            x1 - x0, y1 - y0);
        xcb_request_failed(_basics.connection(), cookie, "Failed to copy area");
        //xcb_flush(_basics.connection());
        xcb_aux_sync(_basics.connection());
    }
}

void Window::terminalChildExited(int exitStatus) throw () {
    PRINT("Child exited: " << exitStatus);
    _open = false;
}
