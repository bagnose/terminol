// vi:noai:sw=4

#ifndef WINDOW__HXX
#define WINDOW__HXX

#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>
#include <cairo-xcb.h>
#include <cairo-ft.h>

class X_Window {
    xcb_connection_t  * _connection;
    xcb_key_symbols_t * _keySymbols;
    xcb_visualtype_t  * _visual;
    xcb_window_t        _window;
    cairo_font_face_t * _fontFace;
    uint16_t            _width;
    uint16_t            _height;
    cairo_surface_t   * _surface;

public:
    X_Window(xcb_connection_t  * connection,
             xcb_screen_t      * screen,
             xcb_key_symbols_t * keySymbols,
             xcb_visualtype_t  * visual,
             cairo_font_face_t * fontFace) :
        _connection(connection),
        _keySymbols(keySymbols),
        _visual(visual),
        _window(0),
        _fontFace(fontFace),
        _width(0),
        _height(0),
        _surface(nullptr)
    {
        uint32_t values[2];
        values[0] = screen->white_pixel;
        values[1] =
            XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE |
            XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_BUTTON_RELEASE |
            //XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
            //XCB_EVENT_MASK_POINTER_MOTION_HINT |
            //XCB_EVENT_MASK_BUTTON_MOTION |
            XCB_EVENT_MASK_EXPOSURE |
            XCB_EVENT_MASK_STRUCTURE_NOTIFY |
            //XCB_EVENT_MASK_FOCUS_CHANGE |
            0;

        _window = xcb_generate_id(_connection);
        xcb_create_window(_connection,
                          XCB_COPY_FROM_PARENT,
                          _window,
                          screen->root,
                          -1, -1,       // x, y     (XXX correct?)
                          320, 240,
                          0,            // border width
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          screen->root_visual,
                          XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                          values);

        xcb_map_window(_connection, _window);
        xcb_flush(_connection);
    }

    ~X_Window() {
        if (_surface) {
            cairo_surface_destroy(_surface);
        }

        xcb_destroy_window(_connection, _window);
    }

    int getFd() { return 0; }
    void read() {}
    bool areWritesQueued() const { return false; }
    void flush() {}

    // Events:

    void keyPress(xcb_key_press_event_t * event) {
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

    void keyRelease(xcb_key_release_event_t * UNUSED(event)) {
    }

    void buttonPress(xcb_button_press_event_t * event) {
        ASSERT(event->event == _window, "Which window?");
        PRINT("Button-press: " << event->event_x << " " << event->event_y);

        xcb_get_geometry_reply_t * geometry =
            xcb_get_geometry_reply(_connection, xcb_get_geometry(_connection, _window), nullptr);

        PRINT("Geometry: " << geometry->x << " " << geometry->y << " " <<
              geometry->width << " " << geometry->height);

        std::free(geometry);
    }

    void buttonRelease(xcb_button_release_event_t * UNUSED(event)) {
    }

    void expose(xcb_expose_event_t * event) {
        ASSERT(event->window == _window, "Which window?");
        PRINT("Expose: " <<
              event->x << " " << event->y << " " <<
              event->width << " " << event->height);

        //draw(event->x, event->y, event->width, event->height);
    }

    void configure(xcb_configure_notify_event_t * event) {
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

protected:
    void draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih) {
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
          cairo_set_font_face(cr, _fontFace);
          cairo_set_font_size(cr, 12.5);

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
};

#endif // WINDOW__HXX
