// vi:noai:sw=4

#include "terminol/xcb/window.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <cairo-ft.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/freetype.h>

#include <unistd.h>
#include <sys/select.h>

class X_EventLoop : protected Uncopyable {
    X_Basics _basics;
    X_Window _window;
public:
    X_EventLoop(cairo_font_face_t           * fontFace,
                const std::string           & term,
                const Interlocutor::Command & command) throw (X_Basics::Error) :
        _basics(),
        _window(_basics.connection(),
                _basics.screen(),
                _basics.keySymbols(),
                _basics.visual(),
                fontFace,
                term,
                command)
    {
        loop();
    }

protected:
    void loop() {
        for (;;) {
            int fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            FD_SET(xcb_get_file_descriptor(_basics.connection()), &readFds);
            fdMax = std::max(fdMax, xcb_get_file_descriptor(_basics.connection()));

            FD_SET(_window.getFd(), &readFds);
            fdMax = std::max(fdMax, _window.getFd());

            bool selectOnWrite = _window.areWritesQueued();
            if (selectOnWrite) {
                FD_SET(_window.getFd(), &writeFds);
                fdMax = std::max(fdMax, _window.getFd());
            }

            //PRINT("Calling select");
            ENFORCE_SYS(TEMP_FAILURE_RETRY(
                ::select(fdMax + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");
            //PRINT("SELECT returned");

            // Handle _one_ I/O. XXX this is sub-optimal

            if (selectOnWrite && FD_ISSET(_window.getFd(), &writeFds)) {
                //PRINT("window write event");
                _window.flush();
            }
            else if (FD_ISSET(xcb_get_file_descriptor(_basics.connection()), &readFds)) {
                //PRINT("xevent");
                xevent();
            }
            else if (FD_ISSET(_window.getFd(), &readFds)) {
                //PRINT("window read event");
                _window.read();
            }
            else {
                FATAL("Unreachable");
            }
        }
    }

protected:
    void xevent() {
        xcb_generic_event_t * event = ::xcb_poll_for_event(_basics.connection());
        if (!event) {
            PRINT("No event!");
            return;
        }
        //bool send_event = XCB_EVENT_SENT(event);
        uint8_t response_type = XCB_EVENT_RESPONSE_TYPE(event);

        ASSERT(response_type != 0, "Error (according to awesome).");

        switch (response_type) {
            case XCB_KEY_PRESS:
                _window.keyPress(reinterpret_cast<xcb_key_press_event_t *>(event));
                break;
            case XCB_KEY_RELEASE:
                _window.keyRelease(reinterpret_cast<xcb_key_release_event_t *>(event));
                break;
            case XCB_BUTTON_PRESS:
                _window.buttonPress(reinterpret_cast<xcb_button_press_event_t *>(event));
                break;
            case XCB_BUTTON_RELEASE:
                _window.buttonRelease(reinterpret_cast<xcb_button_release_event_t *>(event));
                break;
            case XCB_EXPOSE:
                _window.expose(reinterpret_cast<xcb_expose_event_t *>(event));
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
                _window.configureNotify(reinterpret_cast<xcb_configure_notify_event_t *>(event));
                break;
            default:
                PRINT("Unrecognised event: " << static_cast<int>(response_type));
                break;
        }
        std::free(event);
    }
};

//
//
//

bool argMatch(const std::string & arg, const std::string & opt, std::string & val) {
    std::string optComposed = "--" + opt + "=";
    if (arg.substr(0, optComposed.size()) ==  optComposed) {
        val = arg.substr(optComposed.size());
        return true;
    }
    else {
        return false;
    }
}

int main(int argc, char * argv[]) {
    // Command line

    std::string fontName = "inconsolata:pixelsize=24";
    std::string geometryStr;
    std::string term = "ansi";
    Interlocutor::Command command;
    bool         accumulateCommand = false;

    for (int i = 1; i != argc; ++i) {
        std::string arg = argv[i];
        if      (accumulateCommand)                      { command.push_back(arg);   }
        else if (arg == "--execute")                     { accumulateCommand = true; }
        else if (argMatch(arg, "font", fontName))        {}
        else if (argMatch(arg, "term", term))            {}
        else if (argMatch(arg, "geometry", geometryStr)) {}
        else {
            std::cerr
                << "Unrecognised argument '" << arg << "'" << std::endl
                << "Try: --font=FONT --term=TERM --geometry=GEOMETRY --execute ARG0 ARG1..."
                << std::endl;
            return 2;
        }
    }

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

    cairo_font_face_t * fontFace =
        cairo_ft_font_face_create_for_ft_face(ft_face, 0);
    ASSERT(fontFace, "Couldn't load font.");

    try {
        // RAII
        X_EventLoop eventLoop(fontFace, term, command);
    }
    catch (X_Basics::Error & ex) {
        FATAL(ex.message);
    }

    // Global finalisation.

    cairo_font_face_destroy(fontFace);

    FT_Done_Face(ft_face);
    FT_Done_FreeType(ft_library);

    return 0;
}
