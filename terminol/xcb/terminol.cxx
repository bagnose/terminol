// vi:noai:sw=4

#include "terminol/xcb/window.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/common/deduper.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/support/cmdline.hxx"

#include <set>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_aux.h>

#include <unistd.h>
#include <sys/select.h>

class EventLoop :
    protected Window::I_Observer,
    protected Uncopyable
{
    Deduper            _deduper;
    Basics             _basics;
    ColorSet           _colorSet;
    FontManager        _fontManager;
    Window             _window;
    std::set<Window *> _deferrals;

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
        _fontManager(config, _basics),
        _window(*this,
                config,
                _deduper,
                _basics,
                _colorSet,
                _fontManager,
                command)
    {
        loop();
    }

    virtual ~EventLoop() {}

protected:
    void loop() throw (Error) {
        while(_window.isOpen()) {
            auto fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            auto xFd = xcb_get_file_descriptor(_basics.connection());
            auto wFd = _window.getFd();

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

            if (FD_ISSET(xFd, &readFds)) {
                xevent();
            }
            else {
                // XXX For some reason X events can be available even though
                // xFd is not readable. This is effectively polling :(
                xevent();
            }

            // Do the deferrals:

            for (auto window : _deferrals) { window->deferral(); }
            _deferrals.clear();
        }
    }

    void xevent() throw (Error) {
        for (;;) {
            auto event = ::xcb_poll_for_event(_basics.connection());
            if (!event) { break; }

            auto guard        = scopeGuard([event] { std::free(event); });
            auto responseType = XCB_EVENT_RESPONSE_TYPE(event);

            if (responseType == 0) {
                ERROR("Zero response type");
            }
            else {
                dispatch(responseType, event);
            }
        }

        if (xcb_connection_has_error(_basics.connection())) {
            throw Error("Lost display connection.");
        }
    }

    void dispatch(uint8_t responseType, xcb_generic_event_t * event) {
        switch (responseType) {
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
            case XCB_CLIENT_MESSAGE:
                _window.clientMessage(
                        reinterpret_cast<xcb_client_message_event_t *>(event));
                break;
            default:
                PRINT("Unrecognised event: " << static_cast<int>(responseType));
                break;
        }
    }

protected:

    // Window::I_Observer implementation:

    void sync() throw () {
        xcb_aux_sync(_basics.connection());

        for (;;) {
            auto event        = ::xcb_wait_for_event(_basics.connection());
            auto guard        = scopeGuard([event] { std::free(event); });
            auto responseType = XCB_EVENT_RESPONSE_TYPE(event);

            if (responseType == 0) {
                ERROR("Zero response type");
                break;      // Because it could be the configure...?
            }
            else {
                dispatch(responseType, event);
                if (responseType == XCB_CONFIGURE_NOTIFY) {
                    break;
                }
            }
        }
    }

    void defer(Window * window) throw () {
        _deferrals.insert(window);
    }
};

//
//
//

namespace {

std::string makeHelp(const std::string & progName) {
    std::ostringstream ost;
    ost << "terminol " << VERSION << std::endl
        << "Usage: " << progName << " [OPTION]... [--execute COMMAND]" << std::endl
        << std::endl
        << "Options:" << std::endl
        << "  --help" << std::endl
        << "  --version" << std::endl
        << "  --font-name=NAME" << std::endl
        << "  --font-size=SIZE" << std::endl
        << "  --color-scheme=NAME" << std::endl
        << "  --term=NAME" << std::endl
        << "  --trace" << std::endl
        << "  --sync" << std::endl
        ;
    return ost.str();
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;
    parseConfig(config);

    CmdLine cmdLine(makeHelp(argv[0]), VERSION, "--execute");
    cmdLine.add(new StringHandler(config.fontName), '\0', "font-name");
    cmdLine.add(new IntHandler(config.fontSize),    '\0', "font-size");
    cmdLine.add(new BoolHandler(config.traceTty),   '\0', "trace");
    cmdLine.add(new BoolHandler(config.syncTty),    '\0', "sync");
    cmdLine.add(new StringHandler(config.termName), '\0', "term-name");
    cmdLine.add(new_MiscHandler([&](const std::string & name) { config.setColorScheme(name); }), '\0', "color-scheme");

    try {
        auto command = cmdLine.parse(argc, const_cast<const char **>(argv));
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
    catch (const CmdLine::Error & ex) {
        FATAL(ex.message);
    }

    return 0;
}
