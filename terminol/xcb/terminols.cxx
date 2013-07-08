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

#include <set>

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_aux.h>

#include <unistd.h>
#include <sys/select.h>

// For the server FIFO:
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

class I_Creator {
public:
    virtual void create() throw () = 0;
    virtual void shutdown() throw () = 0;

protected:
    I_Creator() {}
    ~I_Creator() {}
};

//
// This server will ultimately be replaced with one based on
// Unix domain sockets.
//

class Server {
    I_Creator    & _creator;
    const Config & _config;
    int            _fd;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Server(I_Creator & creator, const Config & config) :
        _creator(creator),
        _config(config)
    {
        const std::string & socketPath = _config.socketPath;
        if (::mkfifo(socketPath.c_str(), 0600) == -1) {
            switch (errno) {
                case EEXIST:
                    // No problem
                    break;
                default:
                    ERROR("Failed to create fifo: " << socketPath);
            }
        }

        open();
    }

    ~Server() {
        ENFORCE_SYS(::close(_fd) != -1, "");

        /*
        const std::string & socketPath = _config.socketPath;
        if (::unlink(socketPath.c_str() == -1)) {
            ERROR("Failed to remove socket: " << socketPath);
        }
        */
    }

    int getFd() { return _fd; }

    void read() {
        uint8_t buffer[1024];
        auto    rval = TEMP_FAILURE_RETRY(
                ::read(_fd, static_cast<void *>(buffer), sizeof buffer));

        ENFORCE_SYS(rval != -1, "::read() failed");

        if (rval == 0) {
            close();
            open();
        }
        else {
            _creator.create();
        }
    }

protected:
    void open() throw (Error) {
        const std::string & socketPath = _config.socketPath;

        _fd = ::open(socketPath.c_str(), O_RDONLY | O_NONBLOCK);
        if (_fd == -1) {
            ERROR("Failed to open fifo: " << socketPath);
            throw Error("Failed to open fifo");
        }
    }

    void close() {
        ENFORCE_SYS(::close(_fd) != -1, "::close() failed");
    }
};

//
//
//

class EventLoop :
    protected Window::I_Observer,
    protected I_Creator,
    protected Uncopyable
{
    typedef std::map<xcb_window_t, Window *> Windows;

    const Config       & _config;
    Server               _server;         // FIXME what order? socket then X, or other way around?
    Deduper              _deduper;
    Basics               _basics;
    ColorSet             _colorSet;
    FontManager          _fontManager;
    KeyMap               _keyMap;
    Windows              _windows;
    std::set<Window *>   _deferrals;

    bool                 _finished;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    explicit EventLoop(const Config & config)
        throw (Server::Error, Basics::Error, FontSet::Error, Error) :
        _config(config),
        _server(*this, config),
        _deduper(),
        _basics(),
        _colorSet(config, _basics),
        _fontManager(config, _basics),
        _keyMap(),
        _finished(false)
    {
        if (config.serverFork) {
            if (::daemon(1, 0) != -1) {
                throw Error("Failed to daemonise");
            }
        }

        loop();
    }

    virtual ~EventLoop() {
    }

protected:
    void loop() throw (Error) {
        while (!_finished) {
            auto fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            auto xFd = xcb_get_file_descriptor(_basics.connection());
            auto sFd = _server.getFd();

            // Select for read on Server
            FD_SET(sFd, &readFds);
            fdMax = std::max(fdMax, sFd);

            // Select for read on X11
            FD_SET(xFd, &readFds);
            fdMax = std::max(fdMax, xFd);

            // Select for read (and possibly write) on TTYs
            for (auto pair : _windows) {
                auto window = pair.second;
                auto wFd = window->getFd();

                FD_SET(wFd, &readFds);
                if (window->needsFlush()) { FD_SET(wFd, &writeFds); }
                fdMax = std::max(fdMax, wFd);
            }

            ENFORCE_SYS(TEMP_FAILURE_RETRY(
                ::select(fdMax + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");

            // Handle all.

            for (auto pair : _windows) {
                auto window = pair.second;
                auto wFd    = window->getFd();

                if (FD_ISSET(wFd, &writeFds)) { window->flush(); }
                if (FD_ISSET(wFd, &readFds) && window->isOpen()) { window->read(); }
            }

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

            // Service the server.

            if (FD_ISSET(sFd, &readFds)) { _server.read(); }

            // Purge the closed windows.

            std::vector<xcb_window_t> closedIds;

            for (auto pair : _windows) {
                auto window = pair.second;
                if (!window->isOpen()) { closedIds.push_back(window->getWindowId()); }
            }

            for (auto id : closedIds) {
                auto iter = _windows.find(id);
                ASSERT(iter != _windows.end(), "");
                auto window = iter->second;
                delete window;
                _windows.erase(iter);
            }
        }
    }

    void xevent(bool waitForConfigure = false) throw (Error) {
        for (;;) {
            xcb_generic_event_t * event;

            if (waitForConfigure) {
                event = ::xcb_wait_for_event(_basics.connection());
                if (XCB_EVENT_RESPONSE_TYPE(event) == XCB_CONFIGURE_NOTIFY) {
                    waitForConfigure = false;
                }
            }
            else {
                event = ::xcb_poll_for_event(_basics.connection());
                if (!event) { break; }
            }

            ASSERT(event, "Null event");
            auto guard         = scopeGuard([event] { std::free(event); });
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
            case XCB_KEY_PRESS: {
                auto e = reinterpret_cast<xcb_key_press_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->keyPress(e); }
                break;
            }
            case XCB_KEY_RELEASE: {
                auto e = reinterpret_cast<xcb_key_release_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->keyRelease(e); }
                break;
            }
            case XCB_BUTTON_PRESS: {
                auto e = reinterpret_cast<xcb_button_press_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->buttonPress(e); }
                break;
            }
            case XCB_BUTTON_RELEASE: {
                auto e = reinterpret_cast<xcb_button_release_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->buttonRelease(e); }
                break;
            }
            case XCB_MOTION_NOTIFY: {
                auto e = reinterpret_cast<xcb_motion_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->motionNotify(e); }
                break;
            }
            case XCB_EXPOSE: {
                auto e = reinterpret_cast<xcb_expose_event_t *>(event);
                auto i = _windows.find(e->window);
                if (i != _windows.end()) { i->second->expose(e); }
                break;
            }
            case XCB_ENTER_NOTIFY: {
                auto e = reinterpret_cast<xcb_enter_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->enterNotify(e); }
                break;
            }
            case XCB_LEAVE_NOTIFY: {
                auto e = reinterpret_cast<xcb_leave_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->leaveNotify(e); }
                break;
            }
            case XCB_FOCUS_IN: {
                auto e = reinterpret_cast<xcb_focus_in_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->focusIn(e); }
                break;
            }
            case XCB_FOCUS_OUT: {
                auto e = reinterpret_cast<xcb_focus_in_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->focusOut(e); }
                break;
            }
            case XCB_MAP_NOTIFY: {
                auto e = reinterpret_cast<xcb_map_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->mapNotify(e); }
                break;
            }
            case XCB_UNMAP_NOTIFY: {
                auto e = reinterpret_cast<xcb_unmap_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->unmapNotify(e); }
                break;
            }
            case XCB_REPARENT_NOTIFY: {
                auto e = reinterpret_cast<xcb_reparent_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->reparentNotify(e); }
                break;
            }
            case XCB_CONFIGURE_NOTIFY: {
                auto e = reinterpret_cast<xcb_configure_notify_event_t *>(event);
                auto i = _windows.find(e->event);
                if (i != _windows.end()) { i->second->configureNotify(e); }
                break;
            }
            case XCB_VISIBILITY_NOTIFY: {
                auto e = reinterpret_cast<xcb_visibility_notify_event_t *>(event);
                auto i = _windows.find(e->window);
                if (i != _windows.end()) { i->second->visibilityNotify(e); }
                break;
            }
            case XCB_DESTROY_NOTIFY: {
                auto e = reinterpret_cast<xcb_destroy_notify_event_t *>(event);
                auto i = _windows.find(e->window);
                if (i != _windows.end()) { i->second->destroyNotify(e); }
                break;
            }
            case XCB_SELECTION_CLEAR: {
                auto e = reinterpret_cast<xcb_selection_clear_event_t *>(event);
                auto i = _windows.find(e->owner);
                if (i != _windows.end()) { i->second->selectionClear(e); }
                break;
            }
            case XCB_SELECTION_NOTIFY: {
                auto e = reinterpret_cast<xcb_selection_notify_event_t *>(event);
                auto i = _windows.find(e->requestor);
                if (i != _windows.end()) { i->second->selectionNotify(e); }
                break;
            }
            case XCB_SELECTION_REQUEST: {
                auto e = reinterpret_cast<xcb_selection_request_event_t *>(event);
                auto i = _windows.find(e->owner);
                if (i != _windows.end()) { i->second->selectionRequest(e); }
                break;
            }
            case XCB_CLIENT_MESSAGE: {
                auto e = reinterpret_cast<xcb_client_message_event_t *>(event);
                auto i = _windows.find(e->window);
                if (i != _windows.end()) { i->second->clientMessage(e); }
                break;
            }
            default:
                PRINT("Unrecognised event: " << static_cast<int>(responseType));
                break;
        }
    }

    // Window::I_Observer implementation:

    void sync() throw () {
        xcb_aux_sync(_basics.connection());
        xevent(true);
    }

    void defer(Window * window) throw () {
        _deferrals.insert(window);
    }

    // I_Creator implementation:

    void create() throw () {
        try {
            auto window = new Window(*this, _config, _deduper,
                                     _basics, _colorSet, _fontManager, _keyMap);
            auto id     = window->getWindowId();
            _windows.insert(std::make_pair(id, window));
        }
        catch (const Window::Error & ex) {
            PRINT("Failed to create window: " << ex.message);
        }
    }

    void shutdown() throw () {
        _finished = true;
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
        //<< "    " << "--font=FONT --term=TERM --geometry=GEOMETRY \\" << std::endl
        << "    " << "--font=FONT --term=TERM \\" << std::endl
        << "    " << "--double-buffer --trace --sync --execute ARG0 ARG1..."
        << std::endl;
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;
    parseConfig(config);

    // Command line

    for (int i = 1; i != argc; ++i) {
        std::string arg = argv[i];
        std::string val;

        if (arg == "--trace") {
            config.traceTty = true;
        }
        else if (arg == "--sync") {
            config.syncTty = true;
        }
        else if (argMatch(arg, "font", val)) {
            config.fontName = val;
        }
        else if (argMatch(arg, "term", val)) {
            config.termName = val;
        }
        /*
        else if (argMatch(arg, "geometry", val)) {
            // WidthxHeight+XPos+YPos
            config.geometryString = val;
        }
        */
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
        EventLoop eventLoop(config);
    }
    catch (const EventLoop::Error & ex) {
        FATAL(ex.message);
    }
    catch (const FontSet::Error & ex) {
        FATAL(ex.message);
    }
    catch (const Basics::Error & ex) {
        FATAL(ex.message);
    }
    catch (const Server::Error & ex) {
        FATAL(ex.message);
    }

    return 0;
}

