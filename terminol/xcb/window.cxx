// vi:noai:sw=4

#include "terminol/xcb/window.hxx"

const int         X_Window::BORDER_THICKNESS = 1;
const int         X_Window::SCROLLBAR_WIDTH  = 8;
const std::string X_Window::DEFAULT_TITLE    = "terminol";

X_Window::X_Window(xcb_connection_t   * connection,
                   xcb_screen_t       * screen,
                   xcb_key_symbols_t  * keySymbols,
                   xcb_visualtype_t   * visual,
                   X_FontSet          & fontSet,
                   const std::string  & term,
                   const Tty::Command & command) throw (Error) :
    _connection(connection),
    _keySymbols(keySymbols),
    _visual(visual),
    _window(0),
    _fontSet(fontSet),
    _width(0),
    _height(0),
    _tty(nullptr),
    _terminal(nullptr),
    _isOpen(false),
    _pointerRow(std::numeric_limits<uint16_t>::max()),
    _pointerCol(std::numeric_limits<uint16_t>::min()),
    _damage(false),
    _surface(nullptr)
{
    //pangoPlay(); exit(0);

    uint32_t values[2];
    // NOTE: This is an important property because it determines
    // flicker when the window is exposed. Ideally background_pixel
    // should be set to whatever the background of the terminal is.
    values[0] = screen->black_pixel;
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
    xcb_create_window(_connection,
                      XCB_COPY_FROM_PARENT,
                      _window,
                      screen->root,
                      -1, -1,       // x, y     (XXX correct?)
                      _width, _height,
                      0,            // border width
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual,      // XXX is there a "copy from parent" equivalent?
                      XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                      values);

    /*
    XSetStandardProperties(_display, _window,
                           "terminol", "terminol",
                           None, nullptr, 0, nullptr);
                           */

    setTitle(DEFAULT_TITLE);

    xcb_map_window(_connection, _window);
    xcb_flush(_connection);

    _tty = new Tty(rows, cols, stringify(_window), term, command);
    _terminal = new Terminal(*this, *_tty, rows, cols);
    _isOpen = true;
}

X_Window::~X_Window() {
    if (_surface) {
        cairo_surface_destroy(_surface);
    }

    delete _tty;
    delete _terminal;

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

    xcb_keysym_t sym =
        xcb_key_press_lookup_keysym(_keySymbols, event, state_filtered);

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
        PRINT(a);
        //mTty.enqueue(&a, 1);
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

void X_Window::buttonRelease(xcb_button_release_event_t * UNUSED(event)) {
}

void X_Window::motionNotify(xcb_motion_notify_event_t * UNUSED(event)) {
}

void X_Window::mapNotify(xcb_map_notify_event_t * UNUSED(event)) {
}

void X_Window::unmapNotify(xcb_unmap_notify_event_t * UNUSED(event)) {
}

void X_Window::reparentNotify(xcb_reparent_notify_event_t * UNUSED(event)) {
}

void X_Window::expose(xcb_expose_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    PRINT("Expose: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);

    //draw(event->x, event->y, event->width, event->height);
}

void X_Window::configureNotify(xcb_configure_notify_event_t * event) {
    ASSERT(event->window == _window, "Which window?");
    PRINT("Configure notify: " <<
          event->x << " " << event->y << " " <<
          event->width << " " << event->height);

    _width  = event->width;
    _height = event->height;

    if (_surface) {
        cairo_surface_destroy(_surface);
    }

    _surface = cairo_xcb_surface_create(_connection,
                                        _window,
                                        _visual,
                                        _width, _height);

    draw(0, 0, _width, _height);
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

void X_Window::draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih) {
    xcb_clear_area(_connection,
                   0,   // exposures ??
                   _window,
                   ix, iy, iw, ih);


    cairo_t * cr = cairo_create(_surface);

    double x = static_cast<double>(ix);
    double y = static_cast<double>(iy);
    double w = static_cast<double>(iw);
    double h = static_cast<double>(ih);

    cairo_save(cr); {
        //PRINT(<< x << " " << y << " " << w << " " << h);
        cairo_rectangle(cr, x, y, w, h);
        cairo_clip(cr);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
        cairo_set_source_rgba(cr, 0, 0, 0, 1);
        cairo_move_to(cr, 10, 40);
        cairo_set_scaled_font(cr, _fontSet.getNormal());
        //cairo_set_font_size(cr, 24.0);

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
        double h_inc;
        {
            cairo_font_extents_t extents;
            cairo_font_extents(cr, &extents);
            h_inc = extents.height;
            /*
               PRINT("ascent=" << extents.ascent <<
               ", descent=" << extents.descent <<
               ", height=" << extents.height << 
               ", max-x-adv=" << extents.max_x_advance <<
               ", max-y-adv=" << extents.max_y_advance);
               */
        }

        {
            cairo_move_to(cr, 20, 20);
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
            cairo_show_text(cr, "Hello");
#if 0


            //
            //

            cairo_select_font_face(cr, "inconsolata",
                                   CAIRO_FONT_SLANT_NORMAL,
                                   CAIRO_FONT_WEIGHT_NORMAL);
            cairo_move_to(cr, 20, 70);
            cairo_set_source_rgba(cr, 1, 1, 1, 1);
            cairo_show_text(cr, "Hello");
#endif
        }

        /*
           double xx = 1.0;
           double yy = 1.0;

           ASSERT(cairo_status(cr) == 0,
           "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
           for (auto c : mText) {
           yy += h_inc;
           cairo_move_to(cr, xx, yy);
           cairo_show_text(cr, c.c_str());
           }
           */

        ASSERT(cairo_status(cr) == 0,
               "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

    } cairo_restore(cr);
    cairo_destroy(cr);

    xcb_flush(_connection);
}

void X_Window::setTitle(const std::string & UNUSED(title)) {
    NYI("setTitle");
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
    if (_damage /*&& _pixmap*/) {   // XXX
        draw(0, 0, _width, _height);
    }
}
