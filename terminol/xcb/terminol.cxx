// vi:noai:sw=4

#include "terminol/xcb/window.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_set.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/config.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_aux.h>

#include <unistd.h>
#include <sys/select.h>

class EventLoop :
    protected Window::I_Observer,
    protected Uncopyable
{
    Deduper  _deduper;
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

    EventLoop(const Config       & config,
              const Tty::Command & command)
        throw (Basics::Error, FontSet::Error, Window::Error, Error) :
        _deduper(),
        _basics(),
        _colorSet(config, _basics),
        _fontSet(config, _basics),
        _keyMap(),
        _window(*this,
                config,
                _deduper,
                _basics,
                _colorSet,
                _fontSet,
                _keyMap,
                command)
    {
        loop();
    }

    virtual ~EventLoop() {}

protected:
    void loop() throw (Error) {
        while(_window.isOpen()) {
            int fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            int xFd = xcb_get_file_descriptor(_basics.connection());
            int wFd = _window.getFd();

            // Select for read on X11
            FD_SET(xFd, &readFds);
            fdMax = std::max(fdMax, xFd);

            // Select for read (and possibly write) on TTY
            FD_SET(wFd, &readFds);
            if (_window.needsFlush()) { FD_SET(wFd, &writeFds); }
            fdMax = std::max(fdMax, wFd);

            ENFORCE_SYS(TEMP_FAILURE_RETRY(
                ::select(fdMax + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");

            // Handle all.

            if (FD_ISSET(wFd, &writeFds)) { _window.flush(); }

            if (FD_ISSET(wFd, &readFds) && _window.isOpen()) { _window.read(); }

            if (FD_ISSET(xFd, &readFds)) { xevent(); }
            else { /* XXX */ xevent(); }
        }
    }

    void xevent(bool block = false) throw (Error) {
        xcb_generic_event_t * event;

        for (;;) {
            if (block) {
                event = ::xcb_wait_for_event(_basics.connection());
                block = false;
            }
            else {
                event = ::xcb_poll_for_event(_basics.connection());
                if (!event) { break; }
            }

            auto    guard         = scopeGuard([event] { std::free(event); });
            uint8_t response_type = XCB_EVENT_RESPONSE_TYPE(event);
            if (response_type == 0) { throw Error("Lost connection (2)?"); }
            dispatch(response_type & ~0x80, event);
        }

        if (xcb_connection_has_error(_basics.connection())) {
            throw Error("Lost connection (1)?");
        }
    }

    void dispatch(uint8_t response_type, xcb_generic_event_t * event) {
        switch (response_type) {
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
            case XCB_SELECTION_CLEAR:
                _window.selectionClear(
                        reinterpret_cast<xcb_selection_clear_event_t *>(event));
                break;
            case XCB_SELECTION_NOTIFY:
                _window.selectionNotify(
                        reinterpret_cast<xcb_selection_notify_event_t *>(event));
                break;
            case XCB_SELECTION_REQUEST:
                _window.selectionRequest(
                        reinterpret_cast<xcb_selection_request_event_t *>(event));
                break;
            default:
                PRINT("Unrecognised event: " << static_cast<int>(response_type));
                break;
        }
    }

protected:

    // Window::I_Observer implementation:

    void sync() throw () {
        xcb_aux_sync(_basics.connection());
        xevent(true);
    }
};

//
//
//

namespace {

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

void showHelp(const std::string & progName, std::ostream & ost) {
    ost << "Usage:" << std::endl
        << "  " << progName << " \\" << std::endl
        << "    " << "--font=FONT --term=TERM --geometry=GEOMETRY \\" << std::endl
        << "    " << "--double-buffer --trace --sync --execute ARG0 ARG1..."
        << std::endl;
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;
    readConfig(config);

    // Command line

    Tty::Command command;
    bool         accumulateCommand = false;

    for (int i = 1; i != argc; ++i) {
        std::string arg = argv[i];
        std::string val;

        if (accumulateCommand) {
            command.push_back(arg);
        }
        else if (arg == "--execute") {
            accumulateCommand = true;
        }
        else if (arg == "--double-buffer") {
            config.setDoubleBuffer(true);
        }
        else if (arg == "--trace") {
            config.setTraceTty(true);
        }
        else if (arg == "--sync") {
            config.setSyncTty(true);
        }
        else if (argMatch(arg, "font", val)) {
            config.setFontName(val);
        }
        else if (argMatch(arg, "term", val)) {
            config.setTermName(val);
        }
        else if (argMatch(arg, "geometry", val)) {
            // WidthxHeight+XPos+YPos
            config.setGeometryString(val);
        }
        else if (arg == "--help") {
            showHelp(argv[0], std::cout);
            return 0;
        }
        else {
            std::cerr << "Unrecognised argument '" << arg << "'" << std::endl;
            showHelp(argv[0], std::cerr);
            return 2;
        }
    }

    try {
        EventLoop eventLoop(config, command);
    }
    catch (const EventLoop::Error & ex) {
        FATAL(ex.message);
    }
    catch (const Window::Error & ex) {
        FATAL(ex.message);
    }
    catch (const FontSet::Error & ex) {
        FATAL(ex.message);
    }
    catch (const Basics::Error & ex) {
        FATAL(ex.message);
    }

    return 0;
}
