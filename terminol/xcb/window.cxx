// vi:noai:sw=4

#include "terminol/xcb/window.hxx"

#include <xcb/xcb_icccm.h>

const int         Window::BORDER_THICKNESS = 1;
const int         Window::SCROLLBAR_WIDTH  = 8;
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
    _tty(nullptr),
    _terminal(nullptr),
    _isOpen(false),
    _pointerRow(std::numeric_limits<uint16_t>::max()),
    _pointerCol(std::numeric_limits<uint16_t>::max()),
    _damage(false),
    _pixmap(0),
    _surface(nullptr),
    _sync(sync)
{
    xcb_void_cookie_t cookie;

    uint32_t values[2];
    // NOTE: This is an important property because it determines
    // flicker when the window is exposed. Ideally background_pixel
    // should be set to whatever the background of the terminal is.
    values[0] = _colorSet.getBackgroundPixel();
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

    uint16_t rows = 25;
    uint16_t cols = 80;

    _width  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    _height = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();

    _window = xcb_generate_id(_basics.connection());
    cookie = xcb_create_window_checked(_basics.connection(),
                                       _basics.screen()->root_depth,
                                       _window,
                                       _basics.screen()->root,
                                       -1, -1,       // x, y
                                       _width, _height,
                                       0,            // border width
                                       XCB_WINDOW_CLASS_INPUT_OUTPUT,
                                       _basics.screen()->root_visual,
                                       XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                                       values);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to create window")) {
        throw Error("Failed to create window.");
    }

    icccmConfigure();

    setTitle(DEFAULT_TITLE);

    _gc = xcb_generate_id(_basics.connection());
    uint32_t mask = XCB_GC_BACKGROUND | XCB_GC_GRAPHICS_EXPOSURES;
    uint32_t vals[] = { _colorSet.getBackgroundPixel(), 0 };
    cookie = xcb_create_gc_checked(_basics.connection(), _gc, _window, mask, vals);
    if (xcb_request_failed(_basics.connection(), cookie, "Failed to allocate gc")) {
        xcb_destroy_window(_basics.connection(), _window);
        FATAL("");
    }

    xcb_map_window(_basics.connection(), _window);
    xcb_flush(_basics.connection());

    _tty = new Tty(rows, cols, stringify(_window), term, command);
    _terminal = new Terminal(*this, keyMap, *_tty, rows, cols, trace);
    _isOpen = true;
}

Window::~Window() {
    if (_pixmap) {
        xcb_void_cookie_t cookie = xcb_free_pixmap(_basics.connection(), _pixmap);
        if (xcb_request_failed(_basics.connection(), cookie, "Failed to free pixmap")) {
            FATAL("");
        }

        ASSERT(_surface, "");
        cairo_surface_destroy(_surface);
    }

    delete _tty;
    delete _terminal;

    xcb_free_gc(_basics.connection(), _gc);

    if (_window) {
        xcb_destroy_window(_basics.connection(), _window);
    }
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
    //PRINT("Map");
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
                                        _width, _height);
}

void Window::unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) {
    PRINT("UnMap");
    ASSERT(_pixmap, "");
    ASSERT(_surface, "");

    cairo_surface_destroy(_surface);

    xcb_free_pixmap(_basics.connection(), _pixmap);
    _pixmap = 0;
}

void Window::reparentNotify(xcb_reparent_notify_event_t * UNUSED(event)) {
    //PRINT("Reparent");
}

void Window::expose(xcb_expose_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    /*
    PRINT("Expose: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);
          */

    draw(event->x, event->y, event->width, event->height);
}

void Window::configureNotify(xcb_configure_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    /*
    PRINT("Configure notify: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);
          */

    // We are only interested in size changes.
    if (_width == event->width && _height == event->height) { return; }

    _width  = event->width;
    _height = event->height;

    if (_pixmap) {
        ASSERT(_surface, "");

        cairo_surface_destroy(_surface);

        xcb_free_pixmap(_basics.connection(), _pixmap);

        //

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

    if (_isOpen) {
        _tty->resize(rows, cols);
    }

    _terminal->resize(rows, cols);      // Ok to resize if not open?

    if (_pixmap) {
        ASSERT(_surface, "");
        draw(0, 0, _width, _height);
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
                                      2 * BORDER_THICKNESS + _fontSet.getWidth(),
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
    else if (x < _width - BORDER_THICKNESS && y < _height - BORDER_THICKNESS) {
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

void Window::draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih) {
    // Clear the pixmap
    xcb_rectangle_t rectangle = { 0, 0, _width, _height };
    xcb_poly_fill_rectangle(_basics.connection(),
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
        //PRINT(x << " " << y << " " << w << " " << h);
        ///
        cairo_rectangle(cr, x, y, w, h);
        cairo_clip(cr);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

        drawBuffer(cr);
        drawSelection(cr);
        drawCursor(cr);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

    } cairo_restore(cr);
    cairo_destroy(cr);

    cairo_surface_flush(_surface);      // Useful?

    xcb_copy_area(_basics.connection(),
                  _pixmap,
                  _window,
                  _gc,
                  0, 0, // src
                  0, 0, // dst
                  _width, _height);

    xcb_flush(_basics.connection());
}

void Window::drawBuffer(cairo_t * cr) {
    // Declare buffer at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<char> buffer;
    // Reserve the largest amount of memory we could require.
    buffer.reserve(_terminal->buffer().getCols() * utf8::LMAX + 1);

    for (uint16_t r = 0; r != _terminal->buffer().getRows(); ++r) {
        uint16_t     cc = 0;
        uint8_t      fg = 0, bg = 0;
        AttributeSet attrs;

        for (uint16_t c = 0; c != _terminal->buffer().getCols(); ++c) {
            const Cell & cell = _terminal->buffer().getCell(r, c);

            if (buffer.empty() || fg != cell.fg() || bg != cell.bg() || attrs != cell.attrs()) {
                if (!buffer.empty()) {
                    // flush buffer
                    buffer.push_back(NUL);
                    drawUtf8(cr, r, cc, fg, bg, attrs, &buffer.front(), c - cc);
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
                     &buffer.front(), _terminal->buffer().getCols() - cc);
            buffer.clear();
        }
    }
}

void Window::drawSelection(cairo_t * UNUSED(cr)) {
}

void Window::drawUtf8(cairo_t    * cr,
                      uint16_t     row,
                      uint16_t     col,
                      uint8_t      fg,
                      uint8_t      bg,
                      AttributeSet attr,
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
            PRINT("Drawing: " << str);
        }
    }
#endif

    cairo_save(cr); {
        int x, y;
        rowCol2XY(row, col, x, y);

        if (attr.get(Attribute::REVERSE)) { std::swap(fg, bg); }

        cairo_set_scaled_font(cr, _fontSet.get(attr.get(Attribute::BOLD),
                                               attr.get(Attribute::ITALIC)));

        //PRINT("drawing: " << str << " at: " << x << " " << y);

        const Color & bgValues = _colorSet.getIndexedColor(bg);
        cairo_set_source_rgb(cr, bgValues.r, bgValues.g, bgValues.b);
        cairo_rectangle(cr, x, y, count * _fontSet.getWidth(), _fontSet.getHeight());
        cairo_fill(cr);

        const Color & fgValues = _colorSet.getIndexedColor(fg);
        cairo_set_source_rgb(cr, fgValues.r, fgValues.g, fgValues.b);
        cairo_move_to(cr, x, y + _fontSet.getAscent());
        cairo_show_text(cr, str);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
    } cairo_restore(cr);
}

void Window::drawCursor(cairo_t * cr) {
    uint16_t r = _terminal->cursorRow();
    uint16_t c = _terminal->cursorCol();

    const Cell & cell = _terminal->buffer().getCell(r, c);

    drawUtf8(cr,
             r, c,
             cell.bg(), cell.fg(),      // Swap fg/bg for cursor.
             cell.attrs(),
             cell.bytes(), 1);
}

// Terminal::I_Observer implementation:

void Window::terminalBegin() throw () {
}

void Window::terminalResetTitle() throw () {
    setTitle(DEFAULT_TITLE);
}

void Window::terminalSetTitle(const std::string & title) throw () {
    //PRINT("Set title: " << title);
    setTitle(title);
}

void Window::terminalDamageCells(uint16_t UNUSED(row), uint16_t UNUSED(col0), uint16_t UNUSED(col1)) throw () {
    _damage = true;
    if (_sync && _pixmap) {
        draw(0, 0, _width, _height);
    }
}

void Window::terminalDamageAll() throw () {
    _damage = true;
    if (_sync && _pixmap) {
        draw(0, 0, _width, _height);
    }
}

void Window::terminalChildExited(int exitStatus) throw () {
    PRINT("Child exited: " << exitStatus);
    _isOpen = false;
}

void Window::terminalEnd() throw () {
    if (!_sync && _damage && _pixmap) {
        draw(0, 0, _width, _height);
    }
}
