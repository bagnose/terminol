// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#include "terminol/xcb/widget.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/common/deduper.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/server.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pipe.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/support/cmdline.hxx"

#include <set>
#include <map>
#include <memory>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_aux.h>

#include <unistd.h>
#include <sys/select.h>

class EventLoop :
    protected I_Selector::I_ReadHandler,
    protected Widget::I_Observer,
    protected I_Creator,
    protected Uncopyable
{
    const Config                 & _config;
    Tty::Command                   _command;
    Selector                       _selector;
    Pipe                           _pipe;
    Deduper                        _deduper;
    Basics                         _basics;
    Server                         _server;
    ColorSet                       _colorSet;
    FontManager                    _fontManager;
    std::map<xcb_window_t,
        std::unique_ptr<Widget>>   _widgets;
    std::set<Widget *>             _deferrals;
    std::vector<Widget *>          _exits;
    bool                           _finished;

    static EventLoop             * _singleton;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    EventLoop(const Config       & config,
              const Tty::Command & command)
        throw (Basics::Error, Server::Error, Error) :
        _config(config),
        _command(command),
        _selector(),
        _pipe(),
        _deduper(),
        _basics(),
        _server(*this, _selector, config),
        _colorSet(config, _basics),
        _fontManager(config, _basics),
        _widgets(),
        _deferrals(),
        _exits(),
        _finished(false)
    {
        ASSERT(!_singleton, "");
        _singleton = this;

        if (config.serverFork) {
            if (::daemon(1, 1) == -1) {
                throw Error("Failed to daemonise");
            }
        }

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

        while (!_finished) {
            _selector.animate();

            // Poll for X11 events that may not have shown up on the descriptor.
            xevent();

            if (!_exits.empty()) {
                // Purge the exited windows.
                for (auto widget : _exits) {
                    auto iter1 = _deferrals.find(widget);
                    if (iter1 != _deferrals.end()) { _deferrals.erase(iter1); }

                    auto iter2 = _widgets.find(widget->getWindowId());
                    ASSERT(iter2 != _widgets.end(), "");
                    _widgets.erase(iter2);
                }
                _exits.clear();
            }

            // Perform the deferrals.
            for (auto widget : _deferrals) { widget->deferral(); }
            _deferrals.clear();
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

        // It doesn't matter if we read more than one 'death byte' off the pipe
        // because we are going to try to reap *all* the children.
        ENFORCE_SYS(TEMP_FAILURE_RETRY(::read(_pipe.readFd(),
                                              static_cast<void *>(buf), size)) != -1, "");

        for (auto & p : _widgets) {
            auto & w = p.second;
            w->tryReap();
        }
    }

    void dispatch(uint8_t responseType, xcb_generic_event_t * event) {
        switch (responseType) {
            case XCB_KEY_PRESS: {
                auto e = reinterpret_cast<xcb_key_press_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->keyPress(e); }
                break;
            }
            case XCB_KEY_RELEASE: {
                auto e = reinterpret_cast<xcb_key_release_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->keyRelease(e); }
                break;
            }
            case XCB_BUTTON_PRESS: {
                auto e = reinterpret_cast<xcb_button_press_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->buttonPress(e); }
                break;
            }
            case XCB_BUTTON_RELEASE: {
                auto e = reinterpret_cast<xcb_button_release_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->buttonRelease(e); }
                break;
            }
            case XCB_MOTION_NOTIFY: {
                auto e = reinterpret_cast<xcb_motion_notify_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->motionNotify(e); }
                break;
            }
            case XCB_EXPOSE: {
                auto e = reinterpret_cast<xcb_expose_event_t *>(event);
                auto i = _widgets.find(e->window);
                if (i != _widgets.end()) { i->second->expose(e); }
                break;
            }
            case XCB_ENTER_NOTIFY: {
                auto e = reinterpret_cast<xcb_enter_notify_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->enterNotify(e); }
                break;
            }
            case XCB_LEAVE_NOTIFY: {
                auto e = reinterpret_cast<xcb_leave_notify_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->leaveNotify(e); }
                break;
            }
            case XCB_FOCUS_IN: {
                auto e = reinterpret_cast<xcb_focus_in_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->focusIn(e); }
                break;
            }
            case XCB_FOCUS_OUT: {
                auto e = reinterpret_cast<xcb_focus_in_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->focusOut(e); }
                break;
            }
            case XCB_MAP_NOTIFY: {
                auto e = reinterpret_cast<xcb_map_notify_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->mapNotify(e); }
                break;
            }
            case XCB_UNMAP_NOTIFY: {
                auto e = reinterpret_cast<xcb_unmap_notify_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->unmapNotify(e); }
                break;
            }
            case XCB_CONFIGURE_NOTIFY: {
                auto e = reinterpret_cast<xcb_configure_notify_event_t *>(event);
                auto i = _widgets.find(e->event);
                if (i != _widgets.end()) { i->second->configureNotify(e); }
                break;
            }
            case XCB_DESTROY_NOTIFY: {
                auto e = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
                auto i = _widgets.find(e->window);
                if (i != _widgets.end()) { i->second->destroyNotify(e); }
                break;
            }
            case XCB_SELECTION_CLEAR: {
                auto e = reinterpret_cast<xcb_selection_clear_event_t *>(event);
                auto i = _widgets.find(e->owner);
                if (i != _widgets.end()) { i->second->selectionClear(e); }
                break;
            }
            case XCB_SELECTION_NOTIFY: {
                auto e = reinterpret_cast<xcb_selection_notify_event_t *>(event);
                auto i = _widgets.find(e->requestor);
                if (i != _widgets.end()) { i->second->selectionNotify(e); }
                break;
            }
            case XCB_SELECTION_REQUEST: {
                auto e = reinterpret_cast<xcb_selection_request_event_t *>(event);
                auto i = _widgets.find(e->owner);
                if (i != _widgets.end()) { i->second->selectionRequest(e); }
                break;
            }
            case XCB_CLIENT_MESSAGE: {
                auto e = reinterpret_cast<xcb_client_message_event_t *>(event);
                auto i = _widgets.find(e->window);
                if (i != _widgets.end()) { i->second->clientMessage(e); }
                break;
            }
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
                        for (auto & pair : _widgets) {
                            auto & window = pair.second;
                            window->redraw();
                        }
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
        _deferrals.insert(widget);
    }

    void widgetSelected(Widget * widget) throw () override {
        for (auto & p : _widgets) {
            auto & w = p.second;
            if (w.get() != widget) {
                w->clearSelection();
            }
        }
    }

    void widgetReaped(Widget * widget, int UNUSED(status)) throw () override {
        ASSERT(std::find(_exits.begin(), _exits.end(), widget) == _exits.end(), "");
        _exits.push_back(widget);
    }

    // I_Creator implementation:

    void create() throw () override {
        try {
            std::unique_ptr<Widget> widget(
                new Widget(*this, _config, _selector, _deduper,
                           _basics, _colorSet, _fontManager, _command));
            auto id = widget->getWindowId();
            _widgets.insert(std::make_pair(id, std::move(widget)));
        }
        catch (const Widget::Error & ex) {
            PRINT("Failed to create widget: " << ex.message);
        }
    }

    void shutdown() throw () override {
        for (auto & pair : _widgets) {
            pair.second->killReap();
        }

        _finished = true;
    }
};

EventLoop * EventLoop::_singleton = nullptr;

//
//
//

namespace {

std::string makeHelp(const std::string & progName) {
    std::ostringstream ost;
    ost << "terminols " << VERSION << std::endl
        << "Usage: " << progName << " [OPTION]... [--execute COMMAND]" << std::endl
        << std::endl
        << "Options:" << std::endl
        << "  --help" << std::endl
        << "  --version" << std::endl
        << "  --font-name=NAME" << std::endl
        << "  --font-size=SIZE" << std::endl
        << "  --color-scheme=NAME" << std::endl
        << "  --term-name=NAME" << std::endl
        << "  --trace|--no-trace" << std::endl
        << "  --sync|--no-sync" << std::endl
        << "  --socket=SOCKET" << std::endl
        << "  --fork|--no-fork" << std::endl
        ;
    return ost.str();
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;
    parseConfig(config);

    CmdLine cmdLine(makeHelp(argv[0]), VERSION, "--execute");
    cmdLine.add(new StringHandler(config.fontName),   '\0', "font-name");
    cmdLine.add(new IntHandler(config.fontSize),      '\0', "font-size");
    cmdLine.add(new BoolHandler(config.traceTty),     '\0', "trace");
    cmdLine.add(new BoolHandler(config.syncTty),      '\0', "sync");
    cmdLine.add(new BoolHandler(config.traditionalWrapping), '\0', "traditional-wrapping");
    cmdLine.add(new StringHandler(config.termName),   '\0', "term-name");
    cmdLine.add(new StringHandler(config.socketPath), '\0', "socket");
    cmdLine.add(new BoolHandler(config.serverFork),   '\0', "fork");
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
    catch (const Basics::Error & ex) {
        FATAL(ex.message);
    }
    catch (const Server::Error & ex) {
        FATAL(ex.message);
    }
    catch (const CmdLine::Error & ex) {
        FATAL(ex.message);
    }

    return 0;
}

