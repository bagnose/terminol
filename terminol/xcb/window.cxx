// vi:noai:sw=4

#include "terminol/xcb/window.hxx"

#include <xcb/xcb_icccm.h>

const int         X_Window::BORDER_THICKNESS = 1;
const int         X_Window::SCROLLBAR_WIDTH  = 8;
const std::string X_Window::DEFAULT_TITLE    = "terminol";

const double X_Window::COLOURS[16][3] = {
    { 0.0,  0.0,  0.0  }, // black
    { 0.66, 0.0,  0.0  }, // red
    { 0.0,  0.66, 0.0  }, // green
    { 0.66, 0.66, 0.0  }, // orange
    { 0.0,  0.0,  0.66 }, // blue
    { 0.66, 0.0,  0.66 }, // magenta
    { 0.0,  0.66, 0.66 }, // cyan
    { 0.66, 0.66, 0.66 }, // light grey
    { 0.33, 0.33, 0.33 }, // dark grey
    { 1.0,  0.33, 0.33 }, // high red
    { 0.33, 1.0,  0.33 }, // high green
    { 1.0,  1.0,  0.33 }, // high yellow
    { 0.33, 0.33, 1.0  }, // high blue
    { 1.0,  0.33, 1.0  }, // high magenta
    { 0.33, 1.0,  1.0  }, // high cyan
    { 1.0,  1.0,  1.0  }  // white
};

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


X_Window::X_Window(xcb_connection_t   * connection,
                   xcb_screen_t       * screen,
                   xcb_key_symbols_t  * keySymbols,
                   xcb_visualtype_t   * visual,
                   X_FontSet          & fontSet,
                   const std::string  & term,
                   const Tty::Command & command) throw (Error) :
    _connection(connection),
    _screen(screen),
    _keySymbols(keySymbols),
    _visual(visual),
    _fontSet(fontSet),
    _window(0),
    _gc(0),
    _width(0),
    _height(0),
    _tty(nullptr),
    _terminal(nullptr),
    _isOpen(false),
    _pointerRow(std::numeric_limits<uint16_t>::max()),
    _pointerCol(std::numeric_limits<uint16_t>::min()),
    _damage(false),
    _pixmap(0),
    _surface(nullptr)
{
    xcb_void_cookie_t cookie;

    uint32_t values[2];
    // NOTE: This is an important property because it determines
    // flicker when the window is exposed. Ideally background_pixel
    // should be set to whatever the background of the terminal is.
    values[0] = _screen->black_pixel;
    values[1] =
        XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
        XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
        XCB_EVENT_MASK_POINTER_MOTION_HINT |
        XCB_EVENT_MASK_BUTTON_MOTION |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_FOCUS_CHANGE |
        0;

    //uint16_t rows = 25;
    //uint16_t cols = 80;
    uint16_t rows = 6;
    uint16_t cols = 14;

    _width  = 2 * BORDER_THICKNESS + cols * 16 /*_fontSet.getWidth()*/ + SCROLLBAR_WIDTH;
    _height = 2 * BORDER_THICKNESS + rows * 24 /*_fontSet.getHeight()*/;

    _window = xcb_generate_id(_connection);
    cookie = xcb_create_window_checked(_connection,
                                       _screen->root_depth,
                                       _window,
                                       _screen->root,
                                       -1, -1,       // x, y     (XXX correct?)
                                       _width, _height,
                                       0,            // border width
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       _screen->root_visual,  // XXX is there a "copy from parent" equivalent?
                                       XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                                       values);
    if (xcb_request_failed(_connection, cookie, "Failed to create window")) {
        FATAL("");
    }

    /*
    XSetStandardProperties(_display, _window,
                           "terminol", "terminol",
                           None, nullptr, 0, nullptr);
                           */

    setTitle(DEFAULT_TITLE);

    _gc = xcb_generate_id(_connection);
    uint32_t mask = XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t vals[] = { 0 };
    cookie = xcb_create_gc_checked(_connection, _gc, _window, mask, vals);
    if (xcb_request_failed(_connection, cookie, "Failed to  allocate gc")) {
        FATAL("");
    }

    xcb_map_window(_connection, _window);
    xcb_flush(_connection);

    _tty = new Tty(rows, cols, stringify(_window), term, command);
    _terminal = new Terminal(*this, *_tty, rows, cols);
    _isOpen = true;
}

X_Window::~X_Window() {
    if (_pixmap) {
        xcb_void_cookie_t cookie = xcb_free_pixmap(_connection, _pixmap);
        if (xcb_request_failed(_connection, cookie, "Failed to free pixmap")) {
            FATAL("");
        }

        ASSERT(_surface, "");
        cairo_surface_destroy(_surface);
    }

    delete _tty;
    delete _terminal;

    xcb_free_gc(_connection, _gc);

    if (_window) {
        xcb_destroy_window(_connection, _window);
    }
}

// Events:

void X_Window::keyPress(xcb_key_press_event_t * event) {
#if 0
    typedef struct xcb_key_press_event_t {
        uint8_t         response_type; /**<  */
        xcb_keycode_t   detail; /**<  */
        uint16_t        sequence; /**<  */
        xcb_timestamp_t time; /**<  */
        xcb_window_t    root; /**<  */
        xcb_window_t    event; /**<  */
        xcb_window_t    child; /**<  */
        int16_t         root_x; /**<  */
        int16_t         root_y; /**<  */
        int16_t         event_x; /**<  */
        int16_t         event_y; /**<  */
        uint16_t        state; /**<  */
        uint8_t         same_screen; /**<  */
        uint8_t         pad0; /**<  */
    } xcb_key_press_event_t;
#endif

    /* Remove the numlock bit, all other bits are modifiers we can bind to */
    int state_filtered = event->state; //& ~(XCB_MOD_MASK_LOCK);
    /* Only use the lower 8 bits of the state (modifier masks) so that mouse
     *      * button masks are filtered out */
    //state_filtered &= 0xFF;

    xcb_keysym_t sym = xcb_key_press_lookup_keysym(_keySymbols, event, state_filtered);

#if 0
    std::ostringstream modifiers;
    if (event->state & XCB_MOD_MASK_SHIFT)   { modifiers << "SHIFT "; }
    if (event->state & XCB_MOD_MASK_LOCK)    { modifiers << "LOCK "; }
    if (event->state & XCB_MOD_MASK_CONTROL) { modifiers << "CTRL "; }
    if (event->state & XCB_MOD_MASK_1)       { modifiers << "ALT "; }
    if (event->state & XCB_MOD_MASK_2)       { modifiers << "2 "; }
    if (event->state & XCB_MOD_MASK_3)       { modifiers << "3 "; }
    if (event->state & XCB_MOD_MASK_4)       { modifiers << "WIN "; }
    if (event->state & XCB_MOD_MASK_5)       { modifiers << "5 "; }

    PRINT("detail: " << event->detail <<
          ", seq: " << event->sequence <<
          ", state: " << event->state << " " <<
          ", sym: " << sym <<
          ", ascii(int): " << (sym & 0x7f) <<
          ", modifiers: " << modifiers.str());
#endif

    // TODO check sym against shortcuts

    if (true || isascii(sym)) {
        char a = sym & 0x7f;
        PRINT("Got key: " << a);
        _tty->write(&a, 1);
    }
}

void X_Window::keyRelease(xcb_key_release_event_t * UNUSED(event)) {
}

void X_Window::buttonPress(xcb_button_press_event_t * event) {
    ASSERT(event->event == _window, "Which window?");
    PRINT("Button-press: " << event->event_x << " " << event->event_y);

    xcb_get_geometry_reply_t * geometry =
        xcb_get_geometry_reply(_connection, xcb_get_geometry(_connection, _window), nullptr);

    PRINT("Geometry: " << geometry->x << " " << geometry->y << " " <<
          geometry->width << " " << geometry->height);

    std::free(geometry);
}

void X_Window::buttonRelease(xcb_button_release_event_t * event) {
    PRINT("Button-release: " << event->event_x << " " << event->event_y);
}

void X_Window::motionNotify(xcb_motion_notify_event_t * event) {
    PRINT("Motion-notify: " << event->event_x << " " << event->event_y);
}

void X_Window::mapNotify(xcb_map_notify_event_t * UNUSED(event)) {
    PRINT("Map");
    ASSERT(!_pixmap, "");

    _pixmap = xcb_generate_id(_connection);
    xcb_void_cookie_t cookie = xcb_create_pixmap_checked(_connection,
                                                         _screen->root_depth,
                                                         _pixmap,
                                                         _window,
                                                         _width,
                                                         _height);
    if (xcb_request_failed(_connection, cookie, "Failed to create pixmap")) {
        FATAL("");
    }

    _surface = cairo_xcb_surface_create(_connection,
                                        _pixmap,
                                        _visual,
                                        _width, _height);
}

void X_Window::unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) {
    PRINT("UnMap");
    ASSERT(_pixmap, "");
    ASSERT(_surface, "");

    cairo_surface_destroy(_surface);

    xcb_free_pixmap(_connection, _pixmap);
    _pixmap = 0;
}

void X_Window::reparentNotify(xcb_reparent_notify_event_t * UNUSED(event)) {
    PRINT("Reparent");
}

void X_Window::expose(xcb_expose_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    PRINT("Expose: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);

    draw(event->x, event->y, event->width, event->height);
}

void X_Window::configureNotify(xcb_configure_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    PRINT("Configure notify: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);

    _width  = event->width;
    _height = event->height;

    if (_pixmap) {
        ASSERT(_surface, "");

        cairo_surface_destroy(_surface);

        xcb_free_pixmap(_connection, _pixmap);

        //

        _pixmap = xcb_generate_id(_connection);
        xcb_void_cookie_t cookie = xcb_create_pixmap_checked(_connection,
                                                             _screen->root_depth,
                                                             _pixmap,
                                                             _window,
                                                             _width,
                                                             _height);
        if (xcb_request_failed(_connection, cookie, "Failed to create pixmap")) {
            FATAL("");
        }

        _surface = cairo_xcb_surface_create(_connection,
                                            _pixmap,
                                            _visual,
                                            _width, _height);
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

    _tty->resize(rows, cols);
    _terminal->resize(rows, cols);

    if (_pixmap) {
        ASSERT(_surface, "");
        draw(0, 0, _width, _height);
    }
}

void X_Window::focusIn(xcb_focus_in_event_t * UNUSED(event)) {
}

void X_Window::focusOut(xcb_focus_out_event_t * UNUSED(event)) {
}

void X_Window::enterNotify(xcb_enter_notify_event_t * UNUSED(event)) {
}

void X_Window::leaveNotify(xcb_leave_notify_event_t * UNUSED(event)) {
}

void X_Window::visibilityNotify(xcb_visibility_notify_event_t & UNUSED(event)) {
}

void X_Window::destroyNotify(xcb_destroy_notify_event_t & UNUSED(event)) {
    _tty->close();
    _isOpen = false;
    _window = 0;
}

void X_Window::rowCol2XY(uint16_t row, uint16_t col, int & x, int & y) const {
    x = BORDER_THICKNESS + col * _fontSet.getWidth();
    y = BORDER_THICKNESS + row * _fontSet.getHeight();
}

bool X_Window::xy2RowCol(int x, int y, uint16_t & row, uint16_t & col) const {
    if (x < BORDER_THICKNESS || y < BORDER_THICKNESS) {
        // Too left or up.
        return false;
    }
    else if (x < _width - BORDER_THICKNESS && y < _height - BORDER_THICKNESS) {
        // NYI
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

void X_Window::draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih) {
    // Clear the pixmap
    xcb_rectangle_t rectangle = { 0, 0, _width, _height };
    xcb_poly_fill_rectangle(_connection,
                            _pixmap,
                            _gc,
                            1,
                            &rectangle);

    //

    ASSERT(_surface, "");
    cairo_t * cr = cairo_create(_surface);

    double x = static_cast<double>(ix);
    double y = static_cast<double>(iy);
    double w = static_cast<double>(iw);
    double h = static_cast<double>(ih);

    cairo_save(cr); {
        PRINT(x << " " << y << " " << w << " " << h);
        cairo_rectangle(cr, x, y, w, h);
        cairo_clip(cr);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

        drawBuffer(cr);
        drawCursor(cr);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

    } cairo_restore(cr);
    cairo_destroy(cr);

    xcb_copy_area(_connection,
                  _pixmap,
                  _window,
                  _gc,
                  0, 0, // src
                  0, 0, // dst
                  _width, _height);

    xcb_flush(_connection);
}

void X_Window::drawBuffer(cairo_t * cr) {
    // Declare buffer at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<char> buffer;
    // Reserve the largest amount of memory we could require.
    buffer.reserve(_terminal->buffer().getCols() * utf8::LMAX + 1);

    for (uint16_t r = 0; r != _terminal->buffer().getRows(); ++r) {
        uint16_t          cc = 0;
        uint8_t           fg = 0, bg = 0;
        AttributeSet      attrs;

        for (uint16_t c = 0; c != _terminal->buffer().getCols(); ++c) {
            const Cell & cell = _terminal->buffer().getCell(r, c);

            if (buffer.empty() || fg != cell.fg() || bg != cell.bg() || attrs != cell.attrs()) {
                if (!buffer.empty()) {
                    // flush buffer
                    buffer.push_back(NUL);
                    drawUtf8(cr, r, cc, fg, bg, attrs, &buffer.front(), c - cc, buffer.size());
                    buffer.clear();
                }

                cc    = c;
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
            drawUtf8(cr, r, cc, fg, bg, attrs,
                     &buffer.front(), _terminal->buffer().getCols() - cc, buffer.size());
            buffer.clear();
        }
    }
}

void X_Window::drawUtf8(cairo_t    * cr,
                        uint16_t     row,
                        uint16_t     col,
                        uint8_t      fg,
                        uint8_t      bg,
                        AttributeSet attr,
                        const char * str,
                        size_t       count,
                        size_t       UNUSED(size)) {
    cairo_save(cr); {
        ENFORCE(fg < 16 && bg < 16, "Haven't done 256 col yet");

        int x, y;
        rowCol2XY(row, col, x, y);

        if (attr.get(Attribute::REVERSE)) { std::swap(fg, bg); }

        cairo_set_scaled_font(cr, _fontSet.get(attr.get(Attribute::BOLD),
                                               attr.get(Attribute::ITALIC)));

        //PRINT("drawing: " << str << " at: " << x << " " << y);

        const double * bgValues = COLOURS[bg];
        cairo_set_source_rgb(cr, bgValues[0], bgValues[1], bgValues[2]);
        cairo_rectangle(cr, x, y, count * _fontSet.getWidth(), _fontSet.getHeight());
        cairo_fill(cr);

        const double * fgValues = COLOURS[fg];
        cairo_set_source_rgb(cr, fgValues[0], fgValues[1], fgValues[2]);
        cairo_move_to(cr, x, y + _fontSet.getAscent());
        cairo_show_text(cr, str);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
    } cairo_restore(cr);
}

void X_Window::drawCursor(cairo_t * cr) {
    int x, y;
    rowCol2XY(_terminal->cursorRow(), _terminal->cursorCol(), x, y);
    cairo_set_source_rgb(cr, 1.0, 0.0, 1.0);
    cairo_rectangle(cr, x, y, _fontSet.getWidth(), _fontSet.getHeight());
    cairo_fill(cr);
}

void X_Window::setTitle(const std::string & title) {
    NYI("setTitle");

    xcb_icccm_set_wm_name(_connection,
                          _window,
                          XCB_ATOM_STRING,
                          8,
                          title.size(),
                          title.data());
}

// Terminal::I_Observer implementation:

void X_Window::terminalBegin() throw () {
}

void X_Window::terminalDamageCells(uint16_t UNUSED(row), uint16_t UNUSED(col0), uint16_t UNUSED(col1)) throw () {
    _damage = true;
}

void X_Window::terminalDamageAll() throw () {
    _damage = true;
}

void X_Window::terminalResetTitle() throw () {
    setTitle(DEFAULT_TITLE);
}

void X_Window::terminalSetTitle(const std::string & title) throw () {
    //PRINT("Set title: " << title);
    setTitle(title);
}

void X_Window::terminalChildExited(int exitStatus) throw () {
    PRINT("Child exited: " << exitStatus);
    _isOpen = false;
}

void X_Window::terminalEnd() throw () {
    if (_damage && _pixmap) {
        draw(0, 0, _width, _height);
    }
}
