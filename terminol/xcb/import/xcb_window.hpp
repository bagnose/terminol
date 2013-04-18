// vi:noai:sw=4

#ifndef WINDOW__H
#define WINDOW__H

#include "terminal/tty.hpp"

#include <vector>

namespace {

xcb_visualtype_t * get_root_visual_type(xcb_screen_t * s) {
  xcb_visualtype_t * visual_type = nullptr;

  for (xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(s);
       depth_iter.rem;
       xcb_depth_next(&depth_iter))
  {
    xcb_visualtype_iterator_t visual_iter;

    for (visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
         visual_iter.rem;
         xcb_visualtype_next(&visual_iter))
    {
      if (s->root_visual == visual_iter.data->visual_id) {
        visual_type = visual_iter.data;
        break;
      }
    }
  }

  return visual_type;
}

} // namespace {anonymous}

class Window : protected Tty::IObserver {
    xcb_connection_t  * mConnection;
    xcb_key_symbols_t * mKeySymbols;
    xcb_window_t        mWindow;
    xcb_visualtype_t  * mVisual;
    cairo_font_face_t * mFontFace;
    Tty                 mTty;
    uint16_t            mWidth;
    uint16_t            mHeight;
    cairo_surface_t   * mSurface;

    // XXX remove all this stuff:
    typedef std::vector<std::string> Text;
    Text mText;

public:
    Window(xcb_connection_t  * connection,
           xcb_screen_t      * screen,
           xcb_key_symbols_t * keySymbols,
           cairo_font_face_t * font_face) :
        mConnection(connection),
        mKeySymbols(keySymbols),
        mWindow(0),
        mVisual(0),
        mFontFace(font_face),
        mTty(*this),
        mWidth(0),
        mHeight(0),
        mSurface(nullptr),
        mText(1, std::string())
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

        mWindow = xcb_generate_id(mConnection);
        xcb_create_window(mConnection,
                          XCB_COPY_FROM_PARENT,
                          mWindow,
                          screen->root,
                          -1, -1,       // x, y     (XXX correct?)
                          320, 240,
                          0,            // border width
                          XCB_WINDOW_CLASS_INPUT_OUTPUT,
                          screen->root_visual,
                          XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK,
                          values);

        xcb_map_window(mConnection, mWindow);
        xcb_flush(mConnection);

        mVisual = get_root_visual_type(screen);

        mTty.open(80, 24, "1234", "blah-term");
    }

    virtual ~Window() {
        if (mSurface) {
            cairo_surface_destroy(mSurface);
        }

        xcb_destroy_window(mConnection, mWindow);
    }

    bool isOpen() const {
        return mTty.isOpen();
    }

    int getFd() {
        ASSERT(isOpen(),);
        return mTty.getFd();
    }

    void read() {
        mTty.read();
    }

    bool queueEmpty() const {
        return mTty.queueEmpty();
    }

    void write() {
        mTty.write();
    }

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
            xcb_key_press_lookup_keysym(mKeySymbols, event, state_filtered);

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
            mTty.enqueue(&a, 1);
        }
    }

    void keyRelease(xcb_key_release_event_t * event) {
    }

    void buttonPress(xcb_button_press_event_t * event) {
        reinterpret_cast<xcb_button_press_event_t *>(event);
        ASSERT(event->event == mWindow, "Which window?");
        PRINT("Button-press: " << event->event_x << " " << event->event_y);

        xcb_get_geometry_reply_t * geometry =
            xcb_get_geometry_reply(mConnection, xcb_get_geometry(mConnection, mWindow), nullptr);

        PRINT("Geometry: " << geometry->x << " " << geometry->y << " " <<
              geometry->width << " " << geometry->height);

        std::free(geometry);
    }

    void buttonRelease(xcb_button_release_event_t * event) {
    }

    void expose(xcb_expose_event_t * event) {
        ASSERT(event->window == mWindow, "Which window?");
        PRINT("Expose: " <<
              event->x << " " << event->y << " " <<
              event->width << " " << event->height);

        draw(event->x, event->y, event->width, event->height);
    }

    void configure(xcb_configure_notify_event_t * event) {
        ASSERT(event->window == mWindow, "Which window?");
        PRINT("Configure notify: " <<
              event->x << " " << event->y << " " <<
              event->width << " " << event->height);

        mWidth  = event->width;
        mHeight = event->height;

        if (mSurface) {
            cairo_surface_destroy(mSurface);
        }

        mSurface = cairo_xcb_surface_create(mConnection,
                                            mWindow,
                                            mVisual,
                                            mWidth, mHeight);

        draw(0, 0, mWidth, mHeight);
    }

protected:
    void draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih) {
        if (iw == 0 || ih == 0) {
            return;
        }

        xcb_clear_area(mConnection,
                       0,   // exposures ??
                       mWindow,
                       ix, iy, iw, ih);


        cairo_t * cr = cairo_create(mSurface);

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
          cairo_set_font_face(cr, mFontFace);
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

          double xx = 1.0;
          double yy = 1.0;

          ASSERT(cairo_status(cr) == 0,
                 "Cairo error: " << cairo_status_to_string(cairo_status(cr)));
          for (auto c : mText) {
            yy += h_inc;
            cairo_move_to(cr, xx, yy);
            cairo_show_text(cr, c.c_str());
          }

          ASSERT(cairo_status(cr) == 0,
                 "Cairo error: " << cairo_status_to_string(cairo_status(cr)));

        } cairo_restore(cr);
        cairo_destroy(cr);

        xcb_flush(mConnection);
    }

    // Tty::IObserver implementation:

    void readResults(const char * data, size_t length) throw () {
        for (size_t i = 0; i != length; ++i) {
          char c = data[i];
            if (isascii(c)) {
                //PRINT("Got ascii: " << int(c) << ": " << c);

                // XXX total stupid hackery.
                if (c == '\n') {
                  mText.push_back(std::string());
                }
                else if (c == '\b') {
                  if (!mText.back().empty()) {
                    mText.back().pop_back();
                  }
                }
                else {
                  mText.back().push_back(c);
                }

                draw(0, 0, mWidth, mHeight);
            }
            else {
                //PRINT("Got other: " << int(c));
            }
        }
    }

    void childExited(int exitStatus) throw () {
        PRINT("Child exited: " << exitStatus);
    }
};

#endif // WINDOW__H
