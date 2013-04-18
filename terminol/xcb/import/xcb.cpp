// vi:noai:sw=4

#include <string>
#include <deque>
#include <sstream>
#include <map>

#include <cstdlib>
#include <cstdio>

#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_keysyms.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/freetype.h>

#include <cairo-xcb.h>
#include <cairo-ft.h>

#include "terminal/xcb_window.hpp"
#include "terminal/common.hpp"

// Control code: ascii 0-31, e.g. BS/backspace, CR/carriage-return, etc
// Escape sequence: ESC followed by a series of 'ordinary' characters.
//                  e.g. echo -e '\033[0;32mBLUE'
//                       echo -e '\033[5B' (move the cursor down 5 lines)


class SimpleEventLoop {
    xcb_connection_t  * mConnection;
    xcb_screen_t      * mScreen;
    xcb_key_symbols_t * mKeySymbols;
    Window            * mWindow;

public:
    SimpleEventLoop(cairo_font_face_t * font_face) :
        mConnection(nullptr),
        mScreen(nullptr),
        mKeySymbols(nullptr),
        mWindow(nullptr)
    {
        int screenNum;
        mConnection = ::xcb_connect(nullptr, &screenNum);
        ENFORCE(mConnection, "Failed to open display.");

        const xcb_setup_t * setup = ::xcb_get_setup(mConnection);
        xcb_screen_iterator_t screenIter = ::xcb_setup_roots_iterator(setup);
        for (int i = 0; i != screenNum; ++i) { ::xcb_screen_next(&screenIter); }
        mScreen = screenIter.data;

        mKeySymbols = xcb_key_symbols_alloc(mConnection);

        mWindow = new Window(mConnection, mScreen, mKeySymbols, font_face);

        loop();
    }

    ~SimpleEventLoop() {
        delete mWindow;

        xcb_key_symbols_free(mKeySymbols);

        ::xcb_disconnect(mConnection);
    }

protected:
    void loop() {
        while (mWindow->isOpen()) {
            int fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            FD_SET(xcb_get_file_descriptor(mConnection), &readFds);
            fdMax = std::max(fdMax, xcb_get_file_descriptor(mConnection));

            FD_SET(mWindow->getFd(), &readFds);
            fdMax = std::max(fdMax, mWindow->getFd());

            if (!mWindow->queueEmpty()) {
                FD_SET(mWindow->getFd(), &writeFds);
                fdMax = std::max(fdMax, mWindow->getFd());
            }

            ENFORCE_SYS(::select(fdMax + 1, &readFds, nullptr, nullptr, nullptr) != -1,);

            if (FD_ISSET(xcb_get_file_descriptor(mConnection), &readFds)) {
                //PRINT("xevent");
                xevent();
            }

            if (FD_ISSET(mWindow->getFd(), &readFds)) {
                //PRINT("window read event");
                mWindow->read();
                if (!mWindow->isOpen()) {
                    break;
                }
            }

            if (!mWindow->queueEmpty()) {
                if (FD_ISSET(mWindow->getFd(), &writeFds)) {
                    //PRINT("window write event");
                    mWindow->write();
                }
            }
        }
    }

    void xevent() {
        xcb_generic_event_t * event = ::xcb_poll_for_event(mConnection);
        if (!event) {
            PRINT("No event!");
            return;
        }
        //bool send_event = XCB_EVENT_SENT(event);
        uint8_t response_type = XCB_EVENT_RESPONSE_TYPE(event);

        ASSERT(response_type != 0, "Error (according to awesome).");

        switch (response_type) {
            case XCB_KEY_PRESS:
                mWindow->keyPress(reinterpret_cast<xcb_key_press_event_t *>(event));
                break;
            case XCB_KEY_RELEASE:
                mWindow->keyRelease(reinterpret_cast<xcb_key_release_event_t *>(event));
                break;
            case XCB_BUTTON_PRESS:
                mWindow->buttonPress(reinterpret_cast<xcb_button_press_event_t *>(event));
                break;
            case XCB_BUTTON_RELEASE:
                mWindow->buttonRelease(reinterpret_cast<xcb_button_release_event_t *>(event));
                break;
            case XCB_EXPOSE:
                mWindow->expose(reinterpret_cast<xcb_expose_event_t *>(event));
                break;
                /*
            case XCB_MAP_NOTIFY:
                PRINT("Got map notify");
                break;
            case XCB_REPARENT_NOTIFY:
                PRINT("Got reparent notify");
                break;
                */
            case XCB_CONFIGURE_NOTIFY:
                mWindow->configure(reinterpret_cast<xcb_configure_notify_event_t *>(event));
                break;
            default:
                PRINT("Unrecognised event: " << static_cast<int>(response_type));
                break;
        }
        std::free(event);
    }
};

int main() {
    // Global initialisation

    FT_Library ft_library;
    if (FT_Init_FreeType(&ft_library) != 0) {
        FATAL("Freetype init failed.");
    }

    FT_Face ft_face;
    if (FT_New_Face(ft_library, "/usr/share/fonts/TTF/ttf-inconsolata.otf",
                    0, &ft_face) != 0)
    {
        FATAL("FT_New_Face failed.");
    }

    cairo_font_face_t * font_face =
        cairo_ft_font_face_create_for_ft_face(ft_face, 0);
    ASSERT(font_face, "Couldn't load font.");

    // Crank up the instance.

    SimpleEventLoop eventLoop(font_face);

    // Global finalisation.

    cairo_font_face_destroy(font_face);

    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);

    return 0;
}
