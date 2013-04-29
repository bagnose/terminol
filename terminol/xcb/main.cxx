// vi:noai:sw=4

#include "terminol/xcb/window.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_set.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <cairo-ft.h>

#include <fontconfig/fontconfig.h>

#include <unistd.h>
#include <sys/select.h>

class EventLoop : protected Uncopyable {
    Basics   _basics;
    ColorSet _colorSet;
    FontSet  _fontSet;
    KeyMap   _keyMap;
    Window   _window;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    EventLoop(const std::string  & fontName,
              const std::string  & term,
              const Tty::Command & command,
              bool                 trace,
              bool                 sync)
        throw (Basics::Error, FontSet::Error, Window::Error, Error) :
        _basics(),
        _colorSet(_basics),
        _fontSet(fontName),
        _keyMap(_basics.maskShift(),
                _basics.maskAlt(),
                _basics.maskControl()),
        _window(_basics,
                _colorSet,
                _fontSet,
                _keyMap,
                term,
                command,
                trace,
                sync)
    {
        loop();
    }

protected:
    void loop() throw (Error) {
        while(_window.isOpen()) {
            int fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            int wFd = _window.getFd();
            int xFd = xcb_get_file_descriptor(_basics.connection());

            FD_SET(xFd, &readFds);
            fdMax = std::max(fdMax, xFd);

            FD_SET(wFd, &readFds);
            fdMax = std::max(fdMax, wFd);

            bool selectOnWrite = _window.needsFlush();
            if (selectOnWrite) {
                FD_SET(wFd, &writeFds);
                fdMax = std::max(fdMax, wFd);
            }

            ENFORCE_SYS(TEMP_FAILURE_RETRY(
                ::select(fdMax + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");

            // Handle all.

            if (selectOnWrite && FD_ISSET(wFd, &writeFds)) {
                _window.flush();
            }

            if (FD_ISSET(wFd, &readFds)) {
                if (_window.isOpen()) {
                    _window.read();
                }
            }

            if (FD_ISSET(xFd, &readFds)) {
                xevent();
                if (xcb_connection_has_error(_basics.connection())) {
                    throw Error("Lost connection (1)?");
                }
            }
            else {
                // XXX Experiment - xevent() anyway!
                xevent();
            }
        }
    }

protected:
    void xevent() throw (Error) {
        xcb_generic_event_t * event;
        while ((event = ::xcb_poll_for_event(_basics.connection()))) {
            try {
                uint8_t response_type = XCB_EVENT_RESPONSE_TYPE(event);

                if (response_type == 0) {
                    throw Error("Lost connection (2)?");
                }

                switch (response_type & ~0x80) {
                    case XCB_KEY_PRESS:
                        _window.keyPress(
                            reinterpret_cast<xcb_key_press_event_t *>(event));
                        break;
                    case XCB_KEY_RELEASE:
                        _window.keyRelease(
                            reinterpret_cast<xcb_key_release_event_t *>(event));
                        break;
                    case XCB_BUTTON_PRESS:
                        _window.buttonPress(
                            reinterpret_cast<xcb_button_press_event_t *>(event));
                        break;
                    case XCB_BUTTON_RELEASE:
                        _window.buttonRelease(
                            reinterpret_cast<xcb_button_release_event_t *>(event));
                        break;
                    case XCB_MOTION_NOTIFY:
                        _window.motionNotify(
                            reinterpret_cast<xcb_motion_notify_event_t *>(event));
                        break;
                    case XCB_EXPOSE:
                        _window.expose(
                            reinterpret_cast<xcb_expose_event_t *>(event));
                        break;
                    case XCB_GRAPHICS_EXPOSURE:
                        PRINT("Got graphics exposure");
                        break;
                    case XCB_NO_EXPOSURE:
                        PRINT("Got no exposure");
                        break;
                    case XCB_ENTER_NOTIFY:
                        _window.enterNotify(
                            reinterpret_cast<xcb_enter_notify_event_t *>(event));
                        break;
                    case XCB_LEAVE_NOTIFY:
                        _window.leaveNotify(
                            reinterpret_cast<xcb_leave_notify_event_t *>(event));
                        break;
                    case XCB_FOCUS_IN:
                        _window.focusIn(
                            reinterpret_cast<xcb_focus_in_event_t *>(event));
                        break;
                    case XCB_FOCUS_OUT:
                        _window.focusOut(
                            reinterpret_cast<xcb_focus_out_event_t *>(event));
                        break;
                    case XCB_MAP_NOTIFY:
                        _window.mapNotify(
                            reinterpret_cast<xcb_map_notify_event_t *>(event));
                        break;
                    case XCB_UNMAP_NOTIFY:
                        _window.unmapNotify(
                            reinterpret_cast<xcb_unmap_notify_event_t *>(event));
                        break;
                    case XCB_REPARENT_NOTIFY:
                        _window.reparentNotify(
                            reinterpret_cast<xcb_reparent_notify_event_t *>(event));
                        break;
                    case XCB_CONFIGURE_NOTIFY:
                        _window.configureNotify(
                            reinterpret_cast<xcb_configure_notify_event_t *>(event));
                        break;
                    case XCB_VISIBILITY_NOTIFY:
                        _window.visibilityNotify(
                            reinterpret_cast<xcb_visibility_notify_event_t *>(event));
                        break;
                    case XCB_DESTROY_NOTIFY:
                        _window.destroyNotify(
                            reinterpret_cast<xcb_destroy_notify_event_t *>(event));
                        break;
                    default:
                        PRINT("Unrecognised event: " << static_cast<int>(response_type));
                        break;
                }
            }
            catch (...) {
                std::free(event);
                throw;
            }
        }
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

    std::string  fontName          = "inconsolata:pixelsize=18";
    std::string  geometryStr;
    std::string  term              = "xterm";
    bool         trace             = false;
    bool         sync              = false;
    Tty::Command command;
    bool         accumulateCommand = false;

    for (int i = 1; i != argc; ++i) {
        std::string arg = argv[i];
        if      (accumulateCommand)                      { command.push_back(arg);   }
        else if (arg == "--execute")                     { accumulateCommand = true; }
        else if (arg == "--trace")                       { trace = true;             }
        else if (arg == "--sync")                        { sync  = true;             }
        else if (argMatch(arg, "font", fontName))        {}
        else if (argMatch(arg, "term", term))            {}
        else if (argMatch(arg, "geometry", geometryStr)) {}
        else {
            if (arg != "--help") {
                std::cerr
                    << "Unrecognised argument '" << arg << "'" << std::endl;
            }
            std::cerr
                << "Usage:" << std::endl
                << "  " << argv[0] << " \\" << std::endl
                << "    " << "--font=FONT --term=TERM --geometry=GEOMETRY \\" << std::endl
                << "    " << "--trace --sync --execute ARG0 ARG1..."
                << std::endl;
            if (arg != "--help") {
                return 2;
            }
            else {
                return 0;
            }
        }
    }

    FcInit();

    try {
        EventLoop eventLoop(fontName, term, command, trace, sync);
    }
    catch (EventLoop::Error & ex) {
        FATAL(ex.message);
    }
    catch (Window::Error & ex) {
        FATAL(ex.message);
    }
    catch (FontSet::Error & ex) {
        FATAL(ex.message);
    }
    catch (Basics::Error & ex) {
        FATAL(ex.message);
    }

    FcFini();

    return 0;
}
