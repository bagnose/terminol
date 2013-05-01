// vi:noai:sw=4

#include "terminol/xcb/window.hxx"

#include <xcb/xcb_icccm.h>

#include <unistd.h>

const int         Window::BORDER_THICKNESS = 50;
const int         Window::SCROLLBAR_WIDTH  = 0;
const std::string Window::DEFAULT_TITLE    = "terminol";

#define xcb_request_failed(connection, cookie, err_msg) _xcb_request_failed(connection, cookie, err_msg, __LINE__)
namespace {
int _xcb_request_failed(xcb_connection_t * connection, xcb_void_cookie_t cookie,
                        const char * err_msg, int line) {
    xcb_generic_error_t * err;
    if ((err = xcb_request_check(connection, cookie)) != nullptr) {
        fprintf(stderr, "[%s:%d] ERROR: %s. X Error Code: %d\n", __FILE__, line, err_msg, err->error_code);
        return err->error_code;
    }
    return 0;
}
} // namespace {anonymous}


Window::Window(Basics             & basics,
               const ColorSet     & colorSet,
               FontSet            & fontSet,
               const KeyMap       & keyMap,
               const std::string  & term,
               const Tty::Command & command,
               bool                 trace,
               bool                 sync) throw (Error) :
    _basics(basics),
    _colorSet(colorSet),
    _fontSet(fontSet),
    _window(0),
    _gc(0),
    _width(0),
    _height(0),
    _nominalWidth(0),
    _nominalHeight(0),
    _tty(nullptr),
    _terminal(nullptr),
    _isOpen(false),
    _pointerRow(std::numeric_limits<uint16_t>::max()),
    _pointerCol(std::numeric_limits<uint16_t>::max()),
    _pixmap(0),
    _surface(nullptr),
    _sync(sync)
{
    xcb_void_cookie_t cookie;

    // Note, it is important to set XCB_CW_BACK_PIXEL to the actual
    // background colour used by the terminal in order to prevent
    // flicker when the window is exposed.
    uint32_t values[] = {
        // XCB_CW_BACK_PIXEL
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

    uint16_t rows = 25;
    uint16_t cols = 80;

    _nominalWidth  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    _nominalHeight = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();
    _width  = _nominalWidth;
    _height = _nominalHeight;

    _window = xcb_generate_id(_basics.connection());
    cookie = xcb_create_window_checked(_basics.connection(),
                                       _basics.screen()->root_depth,
                                       _window,
                                       _basics.screen()->root,
                                       -1, -1,       // x, y
                                       _nominalWidth, _nominalHeight,
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
                                       values);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to create window")) {
        throw Error("Failed to create window.");
    }

    icccmConfigure();

    setTitle(DEFAULT_TITLE);

    _gc = xcb_generate_id(_basics.connection());
    uint32_t vals[] = {
        _basics.screen()->white_pixel /*_colorSet.getBackgroundPixel()*/,
        _basics.screen()->white_pixel /*_colorSet.getBackgroundPixel()*/,
        0                               // no exposures
    };
    cookie = xcb_create_gc_checked(_basics.connection(),
                                   _gc,
                                   _window,
                                   XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES,
                                   vals);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to allocate gc")) {
        xcb_destroy_window(_basics.connection(), _window);
        FATAL("");
    }

    xcb_map_window(_basics.connection(), _window);
    xcb_flush(_basics.connection());

    _tty = new Tty(rows, cols, stringify(_window), term, command);
    _terminal = new Terminal(*this, keyMap, *_tty, rows, cols, trace);
    _isOpen = true;
    PRINT("Created");
}

Window::~Window() {
    if (_pixmap) {
        ASSERT(_surface, "");
        cairo_surface_destroy(_surface);

        ASSERT(_pixmap, "");
        xcb_void_cookie_t cookie = xcb_free_pixmap(_basics.connection(), _pixmap);
        if (xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap")) {
            FATAL("");
        }
    }
    else {
        ASSERT(!_surface, "");
    }

    // Unwind constructor.

    delete _tty;
    delete _terminal;

    xcb_free_gc(_basics.connection(), _gc);

    // The window may have been destroyed exogenously.
    if (_window) {
        xcb_destroy_window(_basics.connection(), _window);
    }
    PRINT("Destroyed");
}

void Window::read() {
    ASSERT(_isOpen, "");
    _terminal->read();
}

bool Window::needsFlush() const {
    ASSERT(_isOpen, "");
    return _terminal->needsFlush();
}

void Window::flush() {
    ASSERT(_isOpen, "");
    _terminal->flush();
}

// Events:

void Window::keyPress(xcb_key_press_event_t * event) {
    if (!_isOpen) { return; }

    xcb_keysym_t keySym = _basics.getKeySym(event->detail, event->state);
    _terminal->keyPress(keySym, event->state);
}

void Window::keyRelease(xcb_key_release_event_t * UNUSED(event)) {
    if (!_isOpen) { return; }
}

void Window::buttonPress(xcb_button_press_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    PRINT("Button-press: " << event->event_x << " " << event->event_y);
    if (!_isOpen) { return; }

    xcb_get_geometry_reply_t * geometry =
        xcb_get_geometry_reply(_basics.connection(), xcb_get_geometry(_basics.connection(), _window), nullptr);

    PRINT("Geometry: " << geometry->x << " " << geometry->y << " " <<
          geometry->width << " " << geometry->height);

    std::free(geometry);
}

void Window::buttonRelease(xcb_button_release_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    PRINT("Button-release: " << event->event_x << " " << event->event_y);
    if (!_isOpen) { return; }
}

void Window::motionNotify(xcb_motion_notify_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    PRINT("Motion-notify: " << event->event_x << " " << event->event_y);
    if (!_isOpen) { return; }
}

void Window::mapNotify(xcb_map_notify_event_t * UNUSED(event)) {
    PRINT("Map");
    ASSERT(!_pixmap, "");

    _pixmap = xcb_generate_id(_basics.connection());
    xcb_void_cookie_t cookie = xcb_create_pixmap_checked(_basics.connection(),
                                                         _basics.screen()->root_depth,
                                                         _pixmap,
                                                         _window,
                                                         _width,
                                                         _height);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to create pixmap")) {
        FATAL("");
    }

    _surface = cairo_xcb_surface_create(_basics.connection(),
                                        _pixmap,
                                        _basics.visual(),
                                        _width,
                                        _height);
}

void Window::unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) {
    PRINT("UnMap");

    ASSERT(_surface, "");
    cairo_surface_destroy(_surface);
    _surface = nullptr;

    ASSERT(_pixmap, "");
    xcb_void_cookie_t cookie = xcb_free_pixmap(_basics.connection(), _pixmap);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap")) {
        FATAL("");
    }
    _pixmap = 0;
}

void Window::reparentNotify(xcb_reparent_notify_event_t * UNUSED(event)) {
    PRINT("Reparent");
}

void Window::expose(xcb_expose_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    PRINT("Expose: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);

#if 0
    draw(event->x, event->y, event->width, event->height, Damage::EXPOSURE);
#else
    if (event->count == 0) {
        draw(0, 0, _width, _height, Damage::EXPOSURE);
    }
    else {
        PRINT("*** Ignoring exposure");
    }
#endif
}

void Window::configureNotify(xcb_configure_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");

    // We are only interested in size changes.
    if (_width == event->width && _height == event->height) {
        PRINT("*** Ignoring configure");
        return;
    }

    PRINT("Configure notify: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);

    _width  = event->width;
    _height = event->height;

    if (_pixmap) {
        ASSERT(_surface, "");
        cairo_surface_destroy(_surface);
        _surface = nullptr;

        ASSERT(_pixmap, "");
        xcb_void_cookie_t cookie;
        cookie = xcb_free_pixmap_checked(_basics.connection(), _pixmap);
        if (xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap")) {
            FATAL("");
        }
        _pixmap = 0;

        //

        _pixmap = xcb_generate_id(_basics.connection());
        cookie = xcb_create_pixmap_checked(_basics.connection(),
                                           _basics.screen()->root_depth,
                                           _pixmap,
                                           _window,
                                           _width,
                                           _height);
        if (xcb_request_failed(_basics.connection(), cookie, "Failed to create pixmap")) {
            FATAL("");
        }

        _surface = cairo_xcb_surface_create(_basics.connection(),
                                            _pixmap,
                                            _basics.visual(),
                                            _width,
                                            _height);
    }

    uint16_t rows, cols;

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

    _nominalWidth  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    _nominalHeight = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();

    if (_isOpen) {
        _tty->resize(rows, cols);
    }

    _terminal->resize(rows, cols);      // Ok to resize if not open?

    if (_pixmap) {
        ASSERT(_surface, "");
        draw(0, 0, _width, _height, Damage::EXPOSURE);
    }
}

void Window::focusIn(xcb_focus_in_event_t * UNUSED(event)) {
}

void Window::focusOut(xcb_focus_out_event_t * UNUSED(event)) {
}

void Window::enterNotify(xcb_enter_notify_event_t * UNUSED(event)) {
}

void Window::leaveNotify(xcb_leave_notify_event_t * UNUSED(event)) {
}

void Window::visibilityNotify(xcb_visibility_notify_event_t * UNUSED(event)) {
}

void Window::destroyNotify(xcb_destroy_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    PRINT("Destroy notify");

    _tty->close();
    _isOpen = false;
    _window = 0;        // I assume we don't need to call xcb_destroy_window?
}

void Window::icccmConfigure() {
    //
    // machine
    //

    const std::string & hostname = _basics.hostname();
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

    xcb_size_hints_t sizeHints;
    sizeHints.flags = 0;
    xcb_icccm_size_hints_set_min_size(&sizeHints,
                                      2 * BORDER_THICKNESS + _fontSet.getWidth() + SCROLLBAR_WIDTH,
                                      2 * BORDER_THICKNESS + _fontSet.getHeight());
    xcb_icccm_size_hints_set_resize_inc(&sizeHints,
                                        _fontSet.getWidth(),
                                        _fontSet.getHeight());
    xcb_icccm_size_hints_set_base_size(&sizeHints,
                                       2 * BORDER_THICKNESS,
                                       2 * BORDER_THICKNESS);
    xcb_icccm_size_hints_set_win_gravity(&sizeHints, XCB_GRAVITY_NORTH_WEST);
    // XXX or call xcb_icccm_set_wm_normal_hints() ?
    xcb_icccm_set_wm_size_hints(_basics.connection(),
                                _window,
                                XCB_ATOM_WM_NORMAL_HINTS,
                                &sizeHints);

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

void Window::rowCol2XY(uint16_t row, uint16_t col, int & x, int & y) const {
    x = BORDER_THICKNESS + col * _fontSet.getWidth();
    y = BORDER_THICKNESS + row * _fontSet.getHeight();
}

bool Window::xy2RowCol(int x, int y, uint16_t & row, uint16_t & col) const {
    if (x < BORDER_THICKNESS || y < BORDER_THICKNESS) {
        // Too left or up.
        return false;
    }
    else if (x < _nominalWidth  - BORDER_THICKNESS - SCROLLBAR_WIDTH &&
             y < _nominalHeight - BORDER_THICKNESS)
    {
        row = (y - BORDER_THICKNESS) / _fontSet.getHeight();
        col = (x - BORDER_THICKNESS) / _fontSet.getWidth();
        ASSERT(row < _terminal->buffer().getRows(),
               "row is: " << row << ", getRows() is: " << _terminal->buffer().getRows());
        ASSERT(col < _terminal->buffer().getCols(),
               "col is: " << col << ", getCols() is: " << _terminal->buffer().getCols());
        return true;
    }
    else {
        // Too right or down.
        return false;
    }
}

void Window::setTitle(const std::string & title) {
    xcb_icccm_set_wm_name(_basics.connection(),
                          _window,
                          XCB_ATOM_STRING,
                          8,
                          title.size(),
                          title.data());

    xcb_icccm_set_wm_icon_name(_basics.connection(),
                               _window,
                               XCB_ATOM_STRING,
                               8,
                               title.size(),
                               title.data());
}

void Window::draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih, Damage damage) {
    ASSERT(_surface, "");
    cairo_t * cr = cairo_create(_surface);

    // Top left corner of damage.
    double x0 = static_cast<double>(ix);
    double y0 = static_cast<double>(iy);

    // Bottom right corner of damage, constrained by nominal area.
    double x2 = static_cast<double>(std::min<uint16_t>(ix + iw, _nominalWidth));
    double y2 = static_cast<double>(std::min<uint16_t>(iy + ih, _nominalHeight));

    if (damage == Damage::EXPOSURE) {
        if (_width > _nominalWidth || _height > _nominalHeight) {
            // The window manager didn't honour our size base/increment hints.
            cairo_save(cr); {
                const auto & bgValues = _colorSet.getBackgroundColor();
                cairo_set_source_rgb(cr, bgValues.r, bgValues.g, bgValues.b);

                if (_width > _nominalWidth) {
                    // Right vertical strip.
                    cairo_rectangle(cr,
                                    static_cast<double>(_nominalWidth),
                                    0.0,
                                    static_cast<double>(_width - _nominalWidth),
                                    static_cast<double>(_height));
                    cairo_fill(cr);
                }

                if (_height > _nominalHeight) {
                    // Bottom horizontal strip.
                    cairo_rectangle(cr,
                                    0.0,
                                    static_cast<double>(_nominalHeight),
                                    static_cast<double>(_width),
                                    static_cast<double>(_height - _nominalHeight));
                    cairo_fill(cr);
                }
            } cairo_restore(cr);
        }
    }

    cairo_save(cr); {
        cairo_rectangle(cr, x0, y0, x2 - x0, y2 - y0);
        cairo_clip(cr);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

        drawBorder(cr, damage);
        drawScrollBar(cr, damage);
        drawBuffer(cr, damage);
        drawSelection(cr, damage);
        drawCursor(cr, damage);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

    } cairo_restore(cr);
    cairo_destroy(cr);

    cairo_surface_flush(_surface);      // Useful?

    xcb_copy_area(_basics.connection(),
                  _pixmap,
                  _window,
                  _gc,
                  ix, iy, // src
                  ix, iy, // dst
                  iw, ih);

    xcb_flush(_basics.connection());
}

void Window::drawBorder(cairo_t * cr, Damage UNUSED(damage)) {
    if (BORDER_THICKNESS > 0) {
        cairo_save(cr); {
            const auto & bgValues = _colorSet.getBorderColor();
            cairo_set_source_rgb(cr, bgValues.r, bgValues.g, bgValues.b);

            // Left edge.
            cairo_rectangle(cr,
                            0.0,
                            0.0,
                            static_cast<double>(BORDER_THICKNESS),
                            static_cast<double>(_nominalHeight));
            cairo_fill(cr);

            // Right edge.
            cairo_rectangle(cr,
                            static_cast<double>(_nominalWidth - BORDER_THICKNESS),
                            0.0,
                            static_cast<double>(BORDER_THICKNESS),
                            static_cast<double>(_nominalHeight));
            cairo_fill(cr);

            // Top edge.
            cairo_rectangle(cr,
                            0.0,
                            0.0,
                            static_cast<double>(_nominalWidth),
                            static_cast<double>(BORDER_THICKNESS));
            cairo_fill(cr);

            // Bottom edge.
            cairo_rectangle(cr,
                            0.0,
                            static_cast<double>(_nominalHeight - BORDER_THICKNESS),
                            static_cast<double>(_nominalWidth),
                            static_cast<double>(BORDER_THICKNESS));
            cairo_fill(cr);
        } cairo_restore(cr);
    }
}

void Window::drawScrollBar(cairo_t * UNUSED(cr), Damage UNUSED(damage)) {
    // TODO
}

void Window::drawBuffer(cairo_t * cr, Damage damage) {
    // Declare buffer at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<char> buffer;
    // Reserve the largest amount of memory we could require.
    buffer.reserve(_terminal->buffer().getCols() * utf8::LMAX + 1);

    for (uint16_t r = 0; r != _terminal->buffer().getRows(); ++r) {
        uint16_t colBegin, colEnd;

        switch (damage) {
            case Damage::TERMINAL:
                _terminal->buffer().getDamage(r, colBegin, colEnd);
                break;
            case Damage::EXPOSURE:
                // FIXME constrain colBegin and colEnd to exposure
                colBegin = 0;
                colEnd   = _terminal->buffer().getCols();
                break;
        }

        uint16_t     c_;
        uint8_t      fg, bg;
        AttributeSet attrs;

        uint16_t     c;

        for (c = colBegin; c != colEnd; ++c) {
            const Cell & cell = _terminal->buffer().getCell(r, c);

            if (buffer.empty() || fg != cell.fg() || bg != cell.bg() || attrs != cell.attrs()) {
                if (!buffer.empty()) {
                    // flush buffer
                    buffer.push_back(NUL);
                    drawUtf8(cr, r, c_, fg, bg, attrs, &buffer.front(), c - c_);
                    buffer.clear();
                }

                c_    = c;
                fg    = cell.fg();
                bg    = cell.bg();
                attrs = cell.attrs();

                utf8::Length len = utf8::leadLength(cell.lead());
                buffer.resize(len);
                std::copy(cell.bytes(), cell.bytes() + len, &buffer.front());
            }
            else {
                size_t oldSize = buffer.size();
                utf8::Length len = utf8::leadLength(cell.lead());
                buffer.resize(buffer.size() + len);
                std::copy(cell.bytes(), cell.bytes() + len, &buffer[oldSize]);
            }
        }

        if (!buffer.empty()) {
            // flush buffer
            buffer.push_back(NUL);
            drawUtf8(cr, r, c_, fg, bg, attrs, &buffer.front(), c - c_);
            buffer.clear();
        }
    }
}

void Window::drawSelection(cairo_t * UNUSED(cr), Damage UNUSED(damage)) {
}

void Window::drawUtf8(cairo_t    * cr,
                      uint16_t     row,
                      uint16_t     col,
                      uint8_t      fg,
                      uint8_t      bg,
                      AttributeSet attrs,
                      const char * str,
                      size_t       count) {
#if 0
    {
        bool nonSpace = false;
        for (const char * s = str; *s != NUL; ++s) {
            if (*s != SPACE) {
                nonSpace = true;
                break;
            }
        }
        if (nonSpace) {
            PRINT("Drawing: '" << str << "'  count=" << count);
        }
    }
#endif

    cairo_save(cr); {
        if (attrs.get(Attribute::REVERSE)) { std::swap(fg, bg); }

        cairo_set_scaled_font(cr, _fontSet.get(attrs.get(Attribute::BOLD),
                                               attrs.get(Attribute::ITALIC)));

        int x, y;
        rowCol2XY(row, col, x, y);

        const auto & bgValues = _colorSet.getIndexedColor(bg);
        cairo_set_source_rgb(cr, bgValues.r, bgValues.g, bgValues.b);
        cairo_rectangle(cr, x, y, count * _fontSet.getWidth(), _fontSet.getHeight());
        cairo_fill(cr);

        const auto & fgValues = _colorSet.getIndexedColor(fg);
        cairo_set_source_rgb(cr, fgValues.r, fgValues.g, fgValues.b);
        cairo_move_to(cr, x, y + _fontSet.getAscent());
        cairo_show_text(cr, str);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
    } cairo_restore(cr);
}

void Window::drawCursor(cairo_t * cr, Damage UNUSED(damage)) {
    uint16_t row = _terminal->cursorRow();
    uint16_t col = _terminal->cursorCol();

    const Cell & cell = _terminal->buffer().getCell(row, col);
    utf8::Length length = utf8::leadLength(cell.lead());
    char buf[utf8::LMAX + 1];
    std::copy(cell.bytes(), cell.bytes() + length, buf);
    buf[length] = NUL;

    // TODO consult config here.
#if 0
    const auto & fgValues = _colorSet.getIndexedColor(cell.bg());
    const auto & bgValues = _colorSet.getIndexedColor(cell.fg());
#else
    const auto & fgValues = _colorSet.getCursorFgColor();
    const auto & bgValues = _colorSet.getCursorBgColor();
#endif

    cairo_save(cr); {
        const AttributeSet & attrs = cell.attrs();
        cairo_set_scaled_font(cr, _fontSet.get(attrs.get(Attribute::BOLD),
                                               attrs.get(Attribute::ITALIC)));

        int x, y;
        rowCol2XY(row, col, x, y);

        cairo_set_source_rgb(cr, bgValues.r, bgValues.g, bgValues.b);
        cairo_rectangle(cr, x, y, _fontSet.getWidth(), _fontSet.getHeight());
        cairo_fill(cr);

        cairo_set_source_rgb(cr, fgValues.r, fgValues.g, fgValues.b);
        cairo_move_to(cr, x, y + _fontSet.getAscent());
        cairo_show_text(cr, buf);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
    } cairo_restore(cr);
}

// Terminal::I_Observer implementation:

void Window::terminalResetTitle() throw () {
    setTitle(DEFAULT_TITLE);
}

void Window::terminalSetTitle(const std::string & title) throw () {
    //PRINT("Set title: " << title);
    setTitle(title);
}

void Window::terminalFixDamage() throw () {
    if (_pixmap) {
        draw(0, 0, _width, _height, Damage::TERMINAL);
    }
}

void Window::terminalChildExited(int exitStatus) throw () {
    PRINT("Child exited: " << exitStatus);
    _isOpen = false;
}
