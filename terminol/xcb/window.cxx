// vi:noai:sw=4

#include "terminol/xcb/window.hxx"
#include "terminol/xcb/common.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/pattern.hxx"

#include <xcb/xcb_icccm.h>
#include <xcb/xcb_aux.h>
#include <pango/pangocairo.h>

#include <limits>

#include <unistd.h>

Window::Window(I_Observer         & observer,
               const Config       & config,
               I_Selector         & selector,
               I_Deduper          & deduper,
               Basics             & basics,
               const ColorSet     & colorSet,
               FontManager        & fontManager,
               const Tty::Command & command) throw (Error) :
    _observer(observer),
    _config(config),
    _basics(basics),
    _colorSet(colorSet),
    _fontManager(fontManager),
    _fontSet(nullptr),
    _window(0),
    _destroyed(false),
    _gc(0),
    _geometry({0, 0, 0, 0}),
    _deferredGeometry({0, 0, 0, 0}),
    _terminal(nullptr),
    _open(false),
    _pointerPos(HPos::invalid()),
    _mapped(false),
    _pixmap(0),
    _surface(nullptr),
    _cr(nullptr),
    _entitlement(Entitlement::PERMANENT),
    _title(_config.title),
    _icon(_config.icon),
    _primarySelection(),
    _clipboardSelection(),
    _pressed(false),
    _pressCount(0),
    _lastPressTime(0),
    _button(XCB_BUTTON_INDEX_ANY),
    _cursorVisible(true),
    _deferralsAllowed(true),
    _deferred(false),
    _hadDeleteRequest(false)
{
    // Register our object with the font manager.

    _fontSet = _fontManager.addClient(this);
    ASSERT(_fontSet, "Null font-set.");
    auto fontGuard = scopeGuard([&] { _fontManager.removeClient(this); });

    // Calculate what our initial geometry should be. Though the WM
    // may give us something else.

    auto rows             = _config.initialRows;
    auto cols             = _config.initialCols;
    auto border_thickness = _config.borderThickness;
    auto scrollbar_width  = _config.scrollbarVisible ? _config.scrollbarWidth : 0;

    _geometry.width  = 2 * border_thickness + cols * _fontSet->getWidth() + scrollbar_width;
    _geometry.height = 2 * border_thickness + rows * _fontSet->getHeight();

    // A generic cookie for all subsequent checked XCB calls.

    xcb_void_cookie_t cookie;

    // Create the window.

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

    _window = xcb_generate_id(_basics.connection());
    cookie  = xcb_create_window_checked(_basics.connection(),
                                        _basics.screen()->root_depth,
                                        _window,
                                        _basics.screen()->root,
                                        _config.initialX, config.initialY,
                                        _geometry.width, _geometry.height,
                                        0,            // border width
                                        XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                        _basics.screen()->root_visual,
                                        XCB_CW_BACK_PIXEL    |
                                        XCB_CW_BIT_GRAVITY   |
                                        XCB_CW_WIN_GRAVITY   |
                                        XCB_CW_BACKING_STORE |
                                        XCB_CW_SAVE_UNDER    |
                                        XCB_CW_EVENT_MASK    |
                                        XCB_CW_CURSOR,
                                        winValues);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to create window.")) {
        throw Error("Failed to create window.");
    }
    auto windowGuard = scopeGuard([&] { xcb_destroy_window(_basics.connection(), _window); });

    // Possibly set the window's opacity.

    if (_config.x11CompositedTransparency) {
        auto max   = std::numeric_limits<uint32_t>::max();
        auto value = clamp<uint32_t>(_config.x11TransparencyValue * max, 0, max);

        cookie = xcb_change_property_checked(_basics.connection(),
                                             XCB_PROP_MODE_REPLACE,
                                             _window,
                                             _basics.atomNetWmWindowOpacity(),
                                             XCB_ATOM_CARDINAL,
                                             32,
                                             1,
                                             &value);
        xcb_request_failed(_basics.connection(), cookie, "Failed to set opacity.");
    }

    // Do the ICCC jive.

    icccmConfigure();

    // Create the GC.

    uint32_t gcValues[] = {
        _colorSet.getVisualBellPixel(),
        0 // no exposures
    };

    _gc    = xcb_generate_id(_basics.connection());
    cookie = xcb_create_gc_checked(_basics.connection(),
                                   _gc,
                                   _window,
                                   XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES,
                                   gcValues);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to allocate GC")) {
        throw Error("Failed to create GC.");
    }
    auto gcGuard = scopeGuard([&] { xcb_free_gc(_basics.connection(), _window); });

    // Create the TTY and terminal.

    try {
        _terminal = new Terminal(*this, _config, selector, deduper,
                                 rows, cols, stringify(_window), command);
        _open     = true;
    }
    catch (const Tty::Error & ex) {
        throw Error("Failed to create tty: " + ex.message);
    }

    // Update the window title.

    setTitle(_title, true);

    // Map the window.

    cookie = xcb_map_window_checked(_basics.connection(), _window);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to map window")) {
        throw Error("Failed to map window.");
    }

    // Flush our XCB calls.

    xcb_flush(_basics.connection());

    // Dismiss the guards.

    gcGuard.dismiss();
    windowGuard.dismiss();
    fontGuard.dismiss();
}

Window::~Window() {
    if (_mapped) {
        ASSERT(_pixmap, "Null pixmap.");
        ASSERT(_surface, "Null surface.");

        destroySurfaceAndPixmap();
    }
    else {
        ASSERT(!_surface, "Surface not null.");
        ASSERT(!_pixmap, "Pixmap not null.");
    }

    // A generic cookie for all subsequent checked XCB calls.

    xcb_void_cookie_t cookie;

    // Unwind constructor.

    delete _terminal;

    cookie = xcb_free_gc_checked(_basics.connection(), _gc);
    xcb_request_failed(_basics.connection(), cookie, "Failed to free GC.");

    // The window may have been destroyed exogenously.
    if (!_destroyed) {
        cookie = xcb_destroy_window_checked(_basics.connection(), _window);
        xcb_request_failed(_basics.connection(), cookie, "Failed to destroy window.");
    }

    // Flush our XCB calls.

    xcb_flush(_basics.connection());

    // Deregister our object with the font manager.

    _fontManager.removeClient(this);
}

// Events:

void Window::keyPress(xcb_key_press_event_t * event) {
    ASSERT(event->event == _window, "Unexpected window.");

    if (_config.autoHideCursor) {
        // Key presses hide the cursor.
        cursorVisibility(false);
    }

    if (!_open) { return; }

    xcb_keysym_t keySym;
    ModifierSet  modifiers;

    if (_basics.getKeySym(event->detail, event->state, keySym, modifiers)) {
        if (_terminal->keyPress(keySym, modifiers)) {
            if (_hadDeleteRequest) {
                // Key presses clear delete requests that are
                // waiting for confirmation.
                _hadDeleteRequest = false;
            }

            if (_entitlement == Entitlement::TRANSIENT) {
                // Key presses reset transient titles.
                _entitlement = Entitlement::PERMANENT;
                setTitle(_title, true);
            }
        }
    }
}

void Window::keyRelease(xcb_key_release_event_t * UNUSED(event)) {
    // XXX Drop this override?
}

void Window::buttonPress(xcb_button_press_event_t * event) {
    ASSERT(event->event == _window, "Unexpected window.");

    if (_config.autoHideCursor) {
        // Button presses show the cursor.
        cursorVisibility(true);
    }

    if (!_open) { return; }
    if (event->detail < XCB_BUTTON_INDEX_1 || event->detail > XCB_BUTTON_INDEX_5) { return; }

    auto modifiers = _basics.convertState(event->state);

    HPos hpos;
    auto within = xy2Pos(event->event_x, event->event_y, hpos);

    switch (event->detail) {
        case XCB_BUTTON_INDEX_4:
            _terminal->scrollWheel(Terminal::ScrollDir::UP, modifiers, within, hpos.pos);
            return;
        case XCB_BUTTON_INDEX_5:
            _terminal->scrollWheel(Terminal::ScrollDir::DOWN, modifiers, within, hpos.pos);
            return;
    }

    if (_pressed) {
        ASSERT(event->detail != _button, "This button is already pressed.");
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
                                   modifiers, within, hpos);
            return;
        case XCB_BUTTON_INDEX_2:
            _terminal->buttonPress(Terminal::Button::MIDDLE, _pressCount,
                                   modifiers, within, hpos);
            return;
        case XCB_BUTTON_INDEX_3:
            _terminal->buttonPress(Terminal::Button::RIGHT, _pressCount,
                                   modifiers, within, hpos);
            return;
    }
}

void Window::buttonRelease(xcb_button_release_event_t * event) {
    ASSERT(event->event == _window, "Unexpected window.");

    if (_config.autoHideCursor) {
        // Button releases show the cursor.
        cursorVisibility(true);
    }

    if (!_open) { return; }
    if (event->detail < XCB_BUTTON_INDEX_1 || event->detail > XCB_BUTTON_INDEX_5) { return; }


    if (_pressed && _button == event->detail) {
        auto modifiers = _basics.convertState(event->state);
        _terminal->buttonRelease(false, modifiers);
        _pressed = false;
    }
}

void Window::motionNotify(xcb_motion_notify_event_t * event) {
    ASSERT(event->event == _window, "Unexpected window.");

    if (_config.autoHideCursor) {
        // Pointer motion show the cursor.
        cursorVisibility(true);
    }

    if (!_open) { return; }

    int16_t x, y;
    uint16_t mask;

    if (event->detail == XCB_MOTION_HINT) {
        auto cookie = xcb_query_pointer(_basics.connection(), _window);
        auto reply  = xcb_query_pointer_reply(_basics.connection(), cookie, nullptr);
        if (!reply) {
            WARNING("Failed to query pointer.");
            return;
        }
        x    = reply->win_x;
        y    = reply->win_y;
        mask = reply->mask;
        std::free(reply);
    }
    else {
        x    = event->event_x;
        y    = event->event_y;
        mask = event->state;
    }

    HPos hpos;
    auto within = xy2Pos(x, y, hpos);

    if (_pointerPos != hpos) {
        auto modifiers = _basics.convertState(mask);
        _pointerPos = hpos;
        _terminal->pointerMotion(modifiers, within, hpos);
    }

}

void Window::mapNotify(xcb_map_notify_event_t * UNUSED(event)) {
    ASSERT(!_mapped, "Received map notification, but already mapped.");

    _mapped = true;
    createPixmapAndSurface();
}

void Window::unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) {
    ASSERT(_mapped, "Received unmap notification, but not mapped.");

    _mapped = false;
    destroySurfaceAndPixmap();
}

void Window::expose(xcb_expose_event_t * event) {
    // If there is a deferral then our pixmap won't be valid.
    if (_deferred) { return; }

    ASSERT(event->window == _window, "Unexpected window.");
    ASSERT(_mapped, "Received expose event, but not mapped.");

    if (_mapped) {
        ASSERT(_pixmap, "Null pixmap.");
        ASSERT(_surface, "Null surface.");
        copyPixmapToWindow(event->x, event->y, event->width, event->height);
    }
}

void Window::configureNotify(xcb_configure_notify_event_t * event) {
    ASSERT(event->window == _window, "Unexpected window.");

    // Note, once we've had a deferral we don't apply the
    // optimisation: no transparency and just a move -> no-op.
    // This is because we might have a resize followed by a move, for example.
    if (!_deferred && !_config.x11PseudoTransparency) {
        // We are only interested in size changes (not moves).
        if (_geometry.width == event->width && _geometry.height == event->height) {
            return;
        }
    }

    _deferredGeometry.width  = event->width;
    _deferredGeometry.height = event->height;

    auto cookie = xcb_translate_coordinates(_basics.connection(), _window,
                                            _basics.screen()->root, 0, 0);
    auto reply  = xcb_translate_coordinates_reply(_basics.connection(), cookie, nullptr);
    if (reply) {
        _deferredGeometry.x = reply->dst_x;
        _deferredGeometry.y = reply->dst_y;
        std::free(reply);
    }
    else {
        ERROR("Failed to translate coordinates.");
    }

    if (_deferralsAllowed) {
        if (!_deferred) {
            _observer.windowDefer(this);
            _deferred = true;
        }
    }
    else {
        handleConfigure();
    }
}

void Window::focusIn(xcb_focus_in_event_t * UNUSED(event)) {
    _terminal->focusChange(true);
}

void Window::focusOut(xcb_focus_out_event_t * UNUSED(event)) {
    _terminal->focusChange(false);

}

void Window::enterNotify(xcb_enter_notify_event_t * UNUSED(event)) {
    // XXX Drop this override?
}

void Window::leaveNotify(xcb_leave_notify_event_t * event) {
    // XXX total guess that this is how we ensure we release
    // the button (broken grabs)...
    if (event->mode == 2) {
        if (_pressed) {
            _terminal->buttonRelease(true, ModifierSet());
            _pressed = false;
        }
    }
}

void Window::destroyNotify(xcb_destroy_notify_event_t * event) {
    ASSERT(event->window == _window, "Unexpected window.");

    _terminal->killReap();
    _open      = false;
    _destroyed = true;
}

void Window::selectionClear(xcb_selection_clear_event_t * UNUSED(event)) {
    _terminal->clearSelection();
}

void Window::selectionNotify(xcb_selection_notify_event_t * UNUSED(event)) {
    if (!_open) { return; }

    std::vector<uint8_t> content;
    uint32_t             offset = 0;        // 32-bit quantities

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

        auto guard  = scopeGuard([reply] { std::free(reply); });
        auto value  = static_cast<uint8_t *>(xcb_get_property_value(reply));
        auto length = xcb_get_property_value_length(reply);
        if (length == 0) { break; }

        auto oldSize = content.size();
        content.resize(oldSize + length);
        std::copy(value, value + length, content.begin() + oldSize);

        offset += (length + 3) / 4;
    }

    if (!content.empty()) {
        _terminal->paste(&content.front(), content.size());
    }
}

void Window::selectionRequest(xcb_selection_request_event_t * event) {
    ASSERT(event->owner == _window, "Unexpected window.");

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
        xcb_request_failed(_basics.connection(), cookie, "Failed to change property.");
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
            ERROR("Unexpected selection.");
        }

        auto cookie = xcb_change_property_checked(_basics.connection(),
                                                  XCB_PROP_MODE_REPLACE,
                                                  event->requestor,
                                                  event->property,
                                                  event->target,
                                                  8,
                                                  text.length(),
                                                  text.data());
        xcb_request_failed(_basics.connection(), cookie, "Failed to change property.");
        response.property = event->property;
    }

    auto cookie = xcb_send_event_checked(_basics.connection(),
                                         true,
                                         event->requestor,
                                         0,
                                         reinterpret_cast<const char *>(&response));
    xcb_request_failed(_basics.connection(), cookie, "Failed to send event.");

    xcb_flush(_basics.connection());        // Required?
}

void Window::clientMessage(xcb_client_message_event_t * event) {
    if (event->type == _basics.atomWmProtocols()) {
        if (event->data.data32[0] == _basics.atomWmDeleteWindow()) {
            handleDelete();
        }
    }
}

void Window::redraw() {
    if (_mapped) {
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");
        renderPixmap();
        copyPixmapToWindow(0, 0, _geometry.width, _geometry.height);
    }
}

void Window::tryReap() {
    _terminal->tryReap();
}

void Window::clearSelection() {
    _terminal->clearSelection();
}

void Window::deferral() {
    ASSERT(_deferred, "");
    _deferred = false;
    handleConfigure();
}

void Window::icccmConfigure() {
    //
    // machine
    //

    auto & hostname = _basics.hostname();
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

    std::string wm_class =
        std::string("terminol") + '\0' +
        std::string("Terminol") + '\0';
    xcb_icccm_set_wm_class(_basics.connection(), _window,
                           wm_class.size(), wm_class.data());

    //
    // size
    //

    auto border_thickness = _config.borderThickness;
    auto scrollbar_width  = _config.scrollbarVisible ? _config.scrollbarWidth : 0;

    auto base_width  = 2 * border_thickness + scrollbar_width;
    auto base_height = 2 * border_thickness;

    auto min_cols = 8;
    auto min_rows = 2;

    xcb_size_hints_t sizeHints;
    sizeHints.flags = 0;
    xcb_icccm_size_hints_set_min_size(&sizeHints,
                                      base_width  + min_cols * _fontSet->getWidth(),
                                      base_height + min_rows * _fontSet->getHeight());
    xcb_icccm_size_hints_set_base_size(&sizeHints,
                                       base_width,
                                       base_height);
    xcb_icccm_size_hints_set_resize_inc(&sizeHints,
                                        _fontSet->getWidth(),
                                        _fontSet->getHeight());
    xcb_icccm_size_hints_set_win_gravity(&sizeHints, XCB_GRAVITY_NORTH_WEST);
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

    xcb_atom_t wmDeleteWindow = _basics.atomWmDeleteWindow();
    xcb_icccm_set_wm_protocols(_basics.connection(), _window,
                               _basics.atomWmProtocols(),
                               1, &wmDeleteWindow);
}

void Window::pos2XY(Pos pos, int & x, int & y) const {
    ASSERT(pos.row <= _terminal->getRows(), "pos.row=" << pos.row << ", getRows()=" << _terminal->getRows());
    ASSERT(pos.col <= _terminal->getCols(), "pos.col=" << pos.col << ", getCols()=" << _terminal->getCols());

    auto border_thickness = _config.borderThickness;

    x = border_thickness + pos.col * _fontSet->getWidth();
    y = border_thickness + pos.row * _fontSet->getHeight();
}

bool Window::xy2Pos(int x, int y, HPos & hpos) const {
    auto within = true;

    auto border_thickness = _config.borderThickness;

    // x / cols:

    if (x < border_thickness) {
        hpos.pos.col = 0;
        hpos.hand = Hand::LEFT;
        within = false;
    }
    else if (x < border_thickness + _fontSet->getWidth() * _terminal->getCols()) {
        auto xx = x - border_thickness;
        hpos.pos.col = xx / _fontSet->getWidth();
        auto remainder = xx - hpos.pos.col * _fontSet->getWidth();
        hpos.hand = remainder < _fontSet->getWidth() / 2 ? Hand::LEFT : Hand::RIGHT;
        ASSERT(hpos.pos.col < _terminal->getCols(),
               "col is: " << hpos.pos.col << ", getCols() is: " <<
               _terminal->getCols());
    }
    else {
        hpos.pos.col = _terminal->getCols();
        hpos.hand = Hand::LEFT;
        within = false;
    }

    // y / rows:

    if (y < border_thickness) {
        hpos.pos.row = 0;
        within = false;
    }
    else if (y < border_thickness + _fontSet->getHeight() * _terminal->getRows()) {
        auto yy = y - border_thickness;
        hpos.pos.row = yy / _fontSet->getHeight();
        ASSERT(hpos.pos.row < _terminal->getRows(),
               "row is: " << hpos.pos.row << ", getRows() is: " <<
               _terminal->getRows());
    }
    else {
        hpos.pos.row = _terminal->getRows() - 1;
        within = false;
    }

    return within;
}

void Window::setTitle(const std::string & title, bool prependGeometry) {
    std::ostringstream ost;
    if (prependGeometry) {
        ost << "[" << _terminal->getCols() << 'x' << _terminal->getRows() << "] ";
    }
    ost << title;

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

    xcb_flush(_basics.connection());
}

void Window::setIcon(const std::string & icon) {
    ASSERT(_terminal, "Null terminal.");

#if 1
    xcb_icccm_set_wm_icon_name(_basics.connection(),
                               _window,
                               XCB_ATOM_STRING,
                               8,
                               icon.size(),
                               icon.data());
#else
    xcb_ewmh_set_wm_icon_name(_basics.ewmhConnection(),
                              _window,
                              icon.size(),
                              icon.data());
#endif
}

void Window::createPixmapAndSurface() {
    _pixmap = xcb_generate_id(_basics.connection());
    // Note, we create the pixmap against the root window rather than
    // _window to avoid dealing with the case where _window may have been
    // asynchronously destroyed.
    auto cookie = xcb_create_pixmap_checked(_basics.connection(),
                                            _basics.screen()->root_depth,
                                            _pixmap,
                                            _basics.screen()->root,
                                            _geometry.width,
                                            _geometry.height);
    xcb_request_failed(_basics.connection(), cookie, "Failed to create pixmap.");

    _surface = cairo_xcb_surface_create(_basics.connection(),
                                        _pixmap,
                                        _basics.visual(),
                                        _geometry.width,
                                        _geometry.height);
    ENFORCE(_surface, "Failed to create surface.");
    ENFORCE(cairo_surface_status(_surface) == CAIRO_STATUS_SUCCESS,
            "Bad cairo surface status.");

    renderPixmap();
}

void Window::destroySurfaceAndPixmap() {
    cairo_surface_finish(_surface);
    cairo_surface_destroy(_surface);
    _surface = nullptr;

    auto cookie = xcb_free_pixmap_checked(_basics.connection(), _pixmap);
    xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap");
    _pixmap = 0;
}

void Window::renderPixmap() {
    ASSERT(_mapped, "");
    ASSERT(_pixmap, "");
    ASSERT(_surface, "");
    _cr = cairo_create(_surface);
    cairo_set_line_width(_cr, 1.0);

    cairo_save(_cr); {
        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));

        drawBorder();
        _terminal->redraw();

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));

    } cairo_restore(_cr);
    cairo_destroy(_cr);
    _cr = nullptr;

    cairo_surface_flush(_surface);      // Useful?
    ENFORCE(cairo_surface_status(_surface) == CAIRO_STATUS_SUCCESS,
            "Bad cairo surface status.");
}

void Window::drawBorder() {
    auto border_thickness = _config.borderThickness;
    auto scrollbar_width  = _config.scrollbarVisible ? _config.scrollbarWidth : 0;

    auto x0 = 0;
    auto x1 = border_thickness;
    auto x2 = border_thickness + _fontSet->getWidth() * _terminal->getCols();
    auto x3 = _geometry.width - scrollbar_width;

    auto y0 = 0;
    auto y1 = border_thickness;
    auto y2 = border_thickness + _fontSet->getHeight() * _terminal->getRows();
    auto y3 = _geometry.height;

    if (_config.x11PseudoTransparency) {
        auto x = _geometry.x;
        auto y = _geometry.y;

        // Left edge.
        xcb_copy_area(_basics.connection(),
                      _basics.rootPixmap(),
                      _pixmap,
                      _gc,
                      x + x0, y + y0,       // src
                      x0, y0,               // dst
                      x1 - x0, y3 - y0);    // w/h

        // Top edge.
        xcb_copy_area(_basics.connection(),
                      _basics.rootPixmap(),
                      _pixmap,
                      _gc,
                      x + x1, y + y0,       // src
                      x1, y0,               // dst
                      x2 - x1, y1 - y0);    // w/h

        // Right edge.
        xcb_copy_area(_basics.connection(),
                      _basics.rootPixmap(),
                      _pixmap,
                      _gc,
                      x + x2, y + y0,       // src
                      x2, y0,               // dst
                      x3 - x2, y3 - y0);    // w/h

        // Bottom edge.
        xcb_copy_area(_basics.connection(),
                      _basics.rootPixmap(),
                      _pixmap,
                      _gc,
                      x + x1, y + y2,       // src
                      x1, y2,
                      x2 - x1, y3 - y2);

        xcb_flush(_basics.connection());
    }


    cairo_save(_cr); {
        auto alpha =
            _config.x11PseudoTransparency ?
            1.0 - _config.x11TransparencyValue :
            1.0;

        auto & bg = _colorSet.getBorderColor();
        cairo_set_source_rgba(_cr, bg.r, bg.g, bg.b, alpha);

        // Left edge.
        cairo_rectangle(_cr,
                        static_cast<double>(x0),
                        static_cast<double>(y0),
                        static_cast<double>(x1 - x0),
                        static_cast<double>(y3 - y0));
        cairo_fill(_cr);

        // Top edge.
        cairo_rectangle(_cr,
                        static_cast<double>(x1),
                        static_cast<double>(y0),
                        static_cast<double>(x2 - x1),
                        static_cast<double>(y1 - y0));
        cairo_fill(_cr);

        // Right edge.
        cairo_rectangle(_cr,
                        static_cast<double>(x2),
                        static_cast<double>(y0),
                        static_cast<double>(x3 - x2),
                        static_cast<double>(y3 - y0));
        cairo_fill(_cr);

        // Bottom edge.
        cairo_rectangle(_cr,
                        static_cast<double>(x1),
                        static_cast<double>(y2),
                        static_cast<double>(x2 - x1),
                        static_cast<double>(y3 - y2));
        cairo_fill(_cr);
    } cairo_restore(_cr);
}

void Window::copyPixmapToWindow(int x, int y, int w, int h) {
    ASSERT(_mapped, "");
    ASSERT(_pixmap, "");
    // Copy the buffer region and flush.
    xcb_copy_area(_basics.connection(),
                  _pixmap,
                  _window,
                  _gc,
                  x, y,   // src
                  x, y,   // dst
                  w, h);
    xcb_flush(_basics.connection());
}

void Window::handleConfigure() {
    if (_deferredGeometry.width != _geometry.width ||
        _deferredGeometry.height != _geometry.height)
    {
        handleResize();
    }
    else {
        if (_config.x11PseudoTransparency) {
            handleMove();
        }
    }
}

void Window::handleResize() {
    _geometry = _deferredGeometry;

    int16_t rows, cols;
    sizeToRowsCols(rows, cols);

    _terminal->resize(rows, cols);      // OK to resize if not open?

    if (_hadDeleteRequest) {
        // Resizes clear delete requests that are waiting for confirmation.
        _hadDeleteRequest = false;
    }

    if (_entitlement != Entitlement::PENDING) {
        // Resizes reset transient titles if no title is pending.
        _entitlement = Entitlement::PERMANENT;
        setTitle(_title, true);
    }

    if (_mapped) {
        ASSERT(_pixmap, "Null pixmap.");
        ASSERT(_surface, "Null surface.");

        destroySurfaceAndPixmap();
        createPixmapAndSurface();

        copyPixmapToWindow(0, 0, _geometry.width, _geometry.height);
    }
}

void Window::handleMove() {
    ASSERT(_config.x11PseudoTransparency, "");

    _geometry = _deferredGeometry;

    if (_mapped) {
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");
        renderPixmap();
        copyPixmapToWindow(0, 0, _geometry.width, _geometry.height);
    }
}

void Window::resizeToAccommodate(int16_t rows, int16_t cols, bool sync) {
    auto border_thickness = _config.borderThickness;
    auto scrollbar_width  = _config.scrollbarVisible ? _config.scrollbarWidth : 0;

    uint16_t width  = 2 * border_thickness + cols * _fontSet->getWidth() + scrollbar_width;
    uint16_t height = 2 * border_thickness + rows * _fontSet->getHeight();

    if (_geometry.width != width || _geometry.height != height) {
        uint32_t values[] = { width, height };
        auto cookie = xcb_configure_window(_basics.connection(),
                                           _window,
                                           XCB_CONFIG_WINDOW_WIDTH |
                                           XCB_CONFIG_WINDOW_HEIGHT,
                                           values);
        if (!xcb_request_failed(_basics.connection(), cookie,
                               "Failed to configure window.")) {
            if (sync) {
                xcb_flush(_basics.connection());
                _deferralsAllowed = false;
                _observer.windowSync();
                _deferralsAllowed = true;
            }
        }
    }
}

void Window::sizeToRowsCols(int16_t & rows, int16_t & cols) const {
    auto border_thickness = _config.borderThickness;
    auto scrollbar_width  = _config.scrollbarVisible ? _config.scrollbarWidth : 0;

    auto base_width  = 2 * border_thickness + scrollbar_width;
    auto base_height = 2 * border_thickness;

    if (_geometry.width  >= static_cast<uint16_t>(base_width  + _fontSet->getWidth()) &&
        _geometry.height >= static_cast<uint16_t>(base_height + _fontSet->getHeight()))
    {
        int16_t w = _geometry.width  - base_width;
        int16_t h = _geometry.height - base_height;

        rows = h / _fontSet->getHeight();
        cols = w / _fontSet->getWidth();
    }
    else {
        rows = cols = 1;
    }

    ASSERT(rows > 0 && cols > 0, "Rows or cols not positive.");
}

void Window::handleDelete() {
    if (_terminal->hasSubprocess()) {
        if (_hadDeleteRequest) {
            xcb_destroy_window(_basics.connection(), _window);
        }
        else {
            _hadDeleteRequest = true;
            _entitlement      = Entitlement::TRANSIENT;
            setTitle("Process is running, once more to confirm...", false);
        }
    }
    else {
        xcb_destroy_window(_basics.connection(), _window);
    }
}

void Window::cursorVisibility(bool visible) {
    ASSERT(_config.autoHideCursor, "");

    if (_cursorVisible != visible) {
        auto mask   = XCB_CW_CURSOR;
        auto values = visible ? _basics.normalCursor() : _basics.invisibleCursor();
        auto cookie = xcb_change_window_attributes_checked(_basics.connection(),
                                                           _window,
                                                           mask,
                                                           &values);
        xcb_request_failed(_basics.connection(), cookie,
                           "Failed to change window attributes.");

        _cursorVisible = visible;
    }
}

// Terminal::I_Observer implementation:

void Window::terminalGetDisplay(std::string & display) throw () {
    display = _basics.display();
}

void Window::terminalCopy(const std::string & text, Terminal::Selection selection) throw () {
    _observer.windowSelected(this);

    xcb_atom_t atom = XCB_ATOM_NONE;

    switch (selection) {
        case Terminal::Selection::CLIPBOARD:
            atom = _basics.atomClipboard();
            _clipboardSelection = text;
            break;
        case Terminal::Selection::PRIMARY:
            atom = _basics.atomPrimary();
            _primarySelection = text;
            break;
    }

    xcb_set_selection_owner(_basics.connection(), _window, atom, XCB_CURRENT_TIME);
    xcb_flush(_basics.connection());
}

void Window::terminalPaste(Terminal::Selection selection) throw () {
    xcb_atom_t atom = XCB_ATOM_NONE;

    switch (selection) {
        case Terminal::Selection::CLIPBOARD:
            atom = _basics.atomClipboard();
            break;
        case Terminal::Selection::PRIMARY:
            atom = _basics.atomPrimary();
            break;
    }

    xcb_convert_selection(_basics.connection(),
                          _window,
                          atom,
                          _basics.atomUtf8String(),
                          XCB_ATOM_PRIMARY, // property
                          XCB_CURRENT_TIME);

    xcb_flush(_basics.connection());
}

void Window::terminalResizeLocalFont(int delta) throw () {
    _fontManager.localDelta(this, delta);
}

void Window::terminalResizeGlobalFont(int delta) throw () {
    _fontManager.globalDelta(delta);
}

void Window::terminalResetTitleAndIcon() throw () {
    _title = _config.title;
    _icon  = _config.icon;
    setTitle(_title, true);
    setIcon(_icon);
}

void Window::terminalSetWindowTitle(const std::string & str, bool transient) throw () {
    if (transient) {
        _entitlement = Entitlement::TRANSIENT;
        setTitle(str, false);
    }
    else {
        _entitlement = Entitlement::PERMANENT;
        _title = str;
        setTitle(_title, true);
    }
}

void Window::terminalSetIconName(const std::string & str) throw () {
    _icon = str;
    setIcon(_icon);
}

void Window::terminalBell() throw () {
    if (_config.mapOnBell) {
        if (!_mapped) {
            xcb_map_window(_basics.connection(), _window);
        }
    }

    if (_config.urgentOnBell) {
        xcb_icccm_wm_hints_t wmHints;
        wmHints.flags = 0;
        xcb_icccm_wm_hints_set_urgency(&wmHints);
        xcb_icccm_set_wm_hints(_basics.connection(), _window, &wmHints);
    }

    if (_config.audibleBell) {
        xcb_bell(_basics.connection(), 100);
    }

    if (_config.visualBell) {
        if (_mapped) {
            ASSERT(_pixmap, "Null pixmap.");
            ASSERT(_surface, "Null surface.");

            // Fill the window with a solid colour.

            xcb_rectangle_t rect = { 0, 0, _geometry.width, _geometry.height };
            xcb_poly_fill_rectangle(_basics.connection(),
                                    _window,
                                    _gc,
                                    1,
                                    &rect);
            xcb_flush(_basics.connection());

            // Wait a moment.

            ::usleep(_config.visualBellDuration * 1000);

            // Copy the pixmap to the window again.

            copyPixmapToWindow(0, 0, _geometry.width, _geometry.height);
        }
    }
}

void Window::terminalResizeBuffer(int16_t rows, int16_t cols) throw () {
    ASSERT(rows > 0 && cols > 0, "Rows or cols not positive.");
    resizeToAccommodate(rows, cols, true);
}

bool Window::terminalFixDamageBegin() throw () {
    // There is no point fixing damage if the pixmap isn't already "current".
    // It's possible for the pixmap to be valid (because the window was mapped)
    // but not current (because we haven't received an expose event yet).
    if (!_deferred && _mapped) {
        ASSERT(_pixmap, "Null pixmap.");
        ASSERT(_surface, "Null surface.");
        _cr = cairo_create(_surface);
        cairo_set_line_width(_cr, 1.0);
        return true;
    }
    else {
        return false;
    }
}

void Window::terminalDrawBg(Pos     pos,
                            int16_t count,
                            UColor  color) throw () {
    int x, y;
    pos2XY(pos, x, y);

    double w = count * _fontSet->getWidth();
    double h = _fontSet->getHeight();

    if (_config.x11PseudoTransparency) {
        xcb_copy_area(_basics.connection(),
                      _basics.rootPixmap(),
                      _pixmap,
                      _gc,
                      _geometry.x + x, _geometry.y + y,   // src
                      x, y,                               // dst
                      w, h);
    }

    ASSERT(_cr, "");
    cairo_save(_cr); {
        auto alpha =
            _config.x11PseudoTransparency ?
            1.0 - _config.x11TransparencyValue :
            1.0;

        auto bg = getColor(color);
        cairo_set_source_rgba(_cr, bg.r, bg.g, bg.b, alpha);

        cairo_rectangle(_cr, x, y, w, h);
        cairo_fill(_cr);

        ASSERT(cairo_status(_cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(_cr)));
    } cairo_restore(_cr);
}

void Window::terminalDrawFg(Pos             pos,
                            int16_t         count,
                            UColor          color,
                            AttrSet         attrs,
                            const uint8_t * str,
                            size_t          size) throw () {
    ASSERT(_cr, "");

    cairo_save(_cr); {
        auto layout = pango_cairo_create_layout(_cr);
        auto layoutGuard = scopeGuard([&] { g_object_unref(layout); });

        auto font = _fontSet->get(attrs.get(Attr::ITALIC), attrs.get(Attr::BOLD));
        pango_layout_set_font_description(layout, font);
        pango_layout_set_width(layout, -1);

        int x, y;
        pos2XY(pos, x, y);

        double w = count * _fontSet->getWidth();
        double h = _fontSet->getHeight();
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
        auto layoutGuard = scopeGuard([&] { g_object_unref(layout); });

        auto font = _fontSet->get(attrs.get(Attr::ITALIC), attrs.get(Attr::BOLD));
        pango_layout_set_font_description(layout, font);

        pango_layout_set_width(layout, -1);
        pango_layout_set_wrap(layout, PANGO_WRAP_CHAR);

        auto fg = getColor(bg_);
        auto bg = getColor(fg_);

        int x, y;
        pos2XY(pos, x, y);

        if (focused) {
            cairo_set_source_rgb(_cr, bg.r, bg.g, bg.b);
        }
        else {
            cairo_set_source_rgb(_cr, fg.r, fg.g, fg.b);
        }

        cairo_rectangle(_cr, x, y, _fontSet->getWidth(), _fontSet->getHeight());
        cairo_fill(_cr);

        auto alpha = wrapNext ? 0.4 : 0.8;
        cairo_set_source_rgba(_cr, bg.r, bg.g, bg.b, alpha);

        if (focused) {
            cairo_rectangle(_cr, x, y, _fontSet->getWidth(), _fontSet->getHeight());
            cairo_fill(_cr);
            cairo_set_source_rgb(_cr, fg.r, fg.g, fg.b);
        }
        else {
            cairo_rectangle(_cr,
                            x + 0.5, y + 0.5,
                            _fontSet->getWidth() - 1.0, _fontSet->getHeight() - 1.0);
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

void Window::terminalDrawScrollbar(size_t  totalRows,
                                   size_t  historyOffset,
                                   int16_t visibleRows) throw () {
    ASSERT(_cr, "");
    ASSERT(_config.scrollbarVisible, "");

    const auto SCROLLBAR_WIDTH  = _config.scrollbarWidth;

    auto x = _geometry.width - SCROLLBAR_WIDTH;
    auto y = 0;
    auto h = _geometry.height;
    auto w = SCROLLBAR_WIDTH;

    // Draw the gutter.

    if (_config.x11PseudoTransparency) {
        xcb_copy_area(_basics.connection(),
                      _basics.rootPixmap(),
                      _pixmap,
                      _gc,
                      _geometry.x + x, _geometry.y + y,   // src
                      x, y,                               // dst
                      w, h);

        xcb_flush(_basics.connection());
    }

    cairo_save(_cr); {
        auto alpha =
            _config.x11PseudoTransparency ?
            1.0 - _config.x11TransparencyValue :
            1.0;

        auto & bg = _colorSet.getScrollBarBgColor();
        cairo_set_source_rgba(_cr, bg.r, bg.g, bg.b, alpha);

        cairo_rectangle(_cr,
                        static_cast<double>(x),
                        static_cast<double>(y),
                        static_cast<double>(w),
                        static_cast<double>(h));
        cairo_fill(_cr);

        // Draw the bar.

        auto min  = 2.0;        // Minimum height we allow the scrollbar to be.
        auto yBar = static_cast<double>(historyOffset) / static_cast<double>(totalRows) * (h - min);
        auto hBar = static_cast<double>(visibleRows)   / static_cast<double>(totalRows) * (h - min);

        auto & fg = _colorSet.getScrollBarFgColor();
        cairo_set_source_rgb(_cr, fg.r, fg.g, fg.b);

        cairo_rectangle(_cr,
                        static_cast<double>(x + 1),
                        yBar,
                        static_cast<double>(w - 2),
                        hBar + min);
        cairo_fill(_cr);
    } cairo_restore(_cr);
}

void Window::terminalFixDamageEnd(const Region & damage,
                                  bool           scrollBar) throw () {
    ASSERT(_cr, "");

    cairo_destroy(_cr);
    _cr = nullptr;

    cairo_surface_flush(_surface);      // Useful?

    int x0, y0;
    pos2XY(damage.begin, x0, y0);
    int x1, y1;
    pos2XY(damage.end, x1, y1);

    if (scrollBar) {
        // Expand the region to include the scroll bar
        y0 = 0;
        x1 = _geometry.width;
        y1 = _geometry.height;
    }

    copyPixmapToWindow(x0, y0, x1 - x0, y1 - y0);
}

void Window::terminalReaped(int status) throw () {
    _open = false;
    _observer.windowReaped(this, status);
}

// FontManager::I_Client implementation:

void Window::useFontSet(FontSet * fontSet, int delta) throw () {
    _fontSet = fontSet;

    xcb_size_hints_t sizeHints;
    sizeHints.flags = 0;
    xcb_icccm_size_hints_set_resize_inc(&sizeHints,
                                        _fontSet->getWidth(),
                                        _fontSet->getHeight());
    xcb_icccm_set_wm_normal_hints(_basics.connection(),
                                  _window,
                                  &sizeHints);

    // Pass 'true' for sync so that the window has handled the configure
    // event when this function returns.
    _entitlement = Entitlement::PENDING;
    resizeToAccommodate(_terminal->getRows(), _terminal->getCols(), true);

    int16_t rows, cols;
    sizeToRowsCols(rows, cols);

    if (rows != _terminal->getRows() || cols != _terminal->getCols()) {
        _terminal->resize(rows, cols);      // Ok to resize if not open?
    }

    if (_mapped) {
        ASSERT(_pixmap, "");
        ASSERT(_surface, "");
        renderPixmap();
        copyPixmapToWindow(0, 0, _geometry.width, _geometry.height);
    }

    std::ostringstream ost;
    ost << "Font size: " << explicitSign(delta);
    _entitlement = Entitlement::TRANSIENT;
    setTitle(ost.str(), true);
}
