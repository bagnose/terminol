// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/xcb/widget.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/common.hxx"
#include "terminol/common/deduper.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pipe.hxx"
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
    protected I_Selector::I_ReadHandler,
    protected Widget::I_Observer,
    protected Uncopyable
{
    const Config     & _config;
    Selector           _selector;
    Pipe               _pipe;
    Deduper            _deduper;
    Basics             _basics;
    ColorSet           _colorSet;
    FontManager        _fontManager;
    Widget             _widget;
    bool               _deferral;
    bool               _exited;

    static EventLoop * _singleton;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    EventLoop(const Config       & config,
              const Tty::Command & command)
        throw (Basics::Error, Widget::Error, Error) :
        _config(config),
        _selector(),
        _pipe(),
        _deduper(),
        _basics(),
        _colorSet(config, _basics),
        _fontManager(config, _basics),
        _widget(*this,
                config,
                _selector,
                _deduper,
                _basics,
                _colorSet,
                _fontManager,
                command),
        _deferral(false),
        _exited(false)
    {
        ASSERT(!_singleton, "");
        _singleton = this;

        if (_config.x11PseudoTransparency) {
            uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
            xcb_change_window_attributes(_basics.connection(),
                                         _basics.screen()->root,
                                         XCB_CW_EVENT_MASK,
                                         &mask);
        }

        loop();
    }

    virtual ~EventLoop() {
        _singleton = nullptr;
    }

protected:
    static void staticSignalHandler(int sigNum) {
        ASSERT(_singleton, "");
        _singleton->signalHandler(sigNum);
    }

    void signalHandler(int UNUSED(sigNum)) {
        // Don't worry about return value.
        char c = 0;
        TEMP_FAILURE_RETRY(::write(_pipe.writeFd(), &c, 1));
    }

    void loop() throw (Error) {
        auto oldHandler = signal(SIGCHLD, &staticSignalHandler);
        _selector.addReadable(_basics.fd(), this);
        _selector.addReadable(_pipe.readFd(), this);

        while (!_exited) {
            _selector.animate();

            // Poll for X11 events that may not have shown up on the descriptor.
            xevent();

            if (_deferral) { _widget.deferral(); _deferral = false; }
        }

        _selector.removeReadable(_pipe.readFd());
        _selector.removeReadable(_basics.fd());
        signal(SIGCHLD, oldHandler);
    }

    void xevent() throw (Error) {
        for (;;) {
            auto event = ::xcb_poll_for_event(_basics.connection());
            if (!event) { break; }

            auto guard        = scopeGuard([event] { std::free(event); });
            auto responseType = XCB_EVENT_RESPONSE_TYPE(event);

            if (responseType != 0) {
                dispatch(responseType, event);
            }
        }

        auto error = xcb_connection_has_error(_basics.connection());
        if (error != 0) {
            throw Error("Lost display connection, error=" + stringify(error));
        }
    }

    void death() {
        char buf[BUFSIZ];
        auto size = sizeof buf;

        ENFORCE_SYS(TEMP_FAILURE_RETRY(::read(_pipe.readFd(),
                                              static_cast<void *>(buf), size)) != -1, "");

        _widget.tryReap();
    }

    void dispatch(uint8_t responseType, xcb_generic_event_t * event) {
        switch (responseType) {
            case XCB_KEY_PRESS:
                _widget.keyPress(
                        reinterpret_cast<xcb_key_press_event_t *>(event));
                break;
            case XCB_KEY_RELEASE:
                _widget.keyRelease(
                        reinterpret_cast<xcb_key_release_event_t *>(event));
                break;
            case XCB_BUTTON_PRESS:
                _widget.buttonPress(
                        reinterpret_cast<xcb_button_press_event_t *>(event));
                break;
            case XCB_BUTTON_RELEASE:
                _widget.buttonRelease(
                        reinterpret_cast<xcb_button_release_event_t *>(event));
                break;
            case XCB_MOTION_NOTIFY:
                _widget.motionNotify(
                        reinterpret_cast<xcb_motion_notify_event_t *>(event));
                break;
            case XCB_EXPOSE:
                _widget.expose(
                        reinterpret_cast<xcb_expose_event_t *>(event));
                break;
            case XCB_ENTER_NOTIFY:
                _widget.enterNotify(
                        reinterpret_cast<xcb_enter_notify_event_t *>(event));
                break;
            case XCB_LEAVE_NOTIFY:
                _widget.leaveNotify(
                        reinterpret_cast<xcb_leave_notify_event_t *>(event));
                break;
            case XCB_FOCUS_IN:
                _widget.focusIn(
                        reinterpret_cast<xcb_focus_in_event_t *>(event));
                break;
            case XCB_FOCUS_OUT:
                _widget.focusOut(
                        reinterpret_cast<xcb_focus_out_event_t *>(event));
                break;
            case XCB_MAP_NOTIFY:
                _widget.mapNotify(
                        reinterpret_cast<xcb_map_notify_event_t *>(event));
                break;
            case XCB_UNMAP_NOTIFY:
                _widget.unmapNotify(
                        reinterpret_cast<xcb_unmap_notify_event_t *>(event));
                break;
            case XCB_CONFIGURE_NOTIFY:
                _widget.configureNotify(
                        reinterpret_cast<xcb_configure_notify_event_t *>(event));
                break;
            case XCB_DESTROY_NOTIFY:
                _widget.destroyNotify(
                        reinterpret_cast<xcb_destroy_notify_event_t *>(event));
                break;
            case XCB_SELECTION_CLEAR:
                _widget.selectionClear(
                        reinterpret_cast<xcb_selection_clear_event_t *>(event));
                break;
            case XCB_SELECTION_NOTIFY:
                _widget.selectionNotify(
                        reinterpret_cast<xcb_selection_notify_event_t *>(event));
                break;
            case XCB_SELECTION_REQUEST:
                _widget.selectionRequest(
                        reinterpret_cast<xcb_selection_request_event_t *>(event));
                break;
            case XCB_CLIENT_MESSAGE:
                _widget.clientMessage(
                        reinterpret_cast<xcb_client_message_event_t *>(event));
                break;
            case XCB_REPARENT_NOTIFY:
                // ignored
                break;
            case XCB_PROPERTY_NOTIFY:
                if (_config.x11PseudoTransparency) {
                    auto e = reinterpret_cast<xcb_property_notify_event_t *>(event);
                    if (e->window == _basics.screen()->root &&
                        e->atom == _basics.atomXRootPixmapId())
                    {
                        _basics.updateRootPixmap();
                        _widget.redraw();
                    }
                }
                break;
            default:
                // Ignore any events we aren't interested in.
                break;
        }
    }

    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) throw () override {
        if (fd == _basics.fd()) {
            xevent();
        }
        else if (fd == _pipe.readFd()) {
            death();
        }
        else {
            FATAL("Bad fd.");
        }
    }

    // Widget::I_Observer implementation:

    void widgetSync() throw () override {
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

    void widgetDefer(Widget * widget) throw () override {
        ASSERT(widget == &_widget, "");
        _deferral = true;
    }

    void widgetSelected(Widget * UNUSED(widget)) throw () override {
        // Nothing to do.
    }

    void widgetReaped(Widget * widget, int UNUSED(status)) throw () override {
        ASSERT(widget == &_widget, "");
        _exited = true;
    }
};

EventLoop * EventLoop::_singleton = nullptr;

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
        << "  --term-name=NAME" << std::endl
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
    cmdLine.add(new BoolHandler(config.traditionalWrapping), '\0', "traditional-wrapping");
    cmdLine.add(new StringHandler(config.termName), '\0', "term-name");
    cmdLine.add(new_MiscHandler([&](const std::string & name) {
                                try {
                                    config.setColorScheme(name);
                                }
                                catch (const ParseError & ex) {
                                    throw CmdLine::Handler::Error(ex.message);
                                }
                                }), '\0', "color-scheme");

    try {
        auto command = cmdLine.parse(argc, const_cast<const char **>(argv));
        EventLoop eventLoop(config, command);
    }
    catch (const EventLoop::Error & ex) {
        FATAL(ex.message);
    }
    catch (const Widget::Error & ex) {
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
