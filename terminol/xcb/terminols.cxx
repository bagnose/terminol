// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#include "terminol/xcb/screen.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/common.hxx"
#include "terminol/xcb/dispatcher.hxx"
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
    protected Screen::I_Observer,
    protected I_Dispatcher::I_Observer,
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
    Dispatcher                     _dispatcher;
    std::map<xcb_window_t,
        std::unique_ptr<Screen>>   _screens;
    std::set<Screen *>             _deferrals;
    std::vector<Screen *>          _exits;
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
        _dispatcher(_selector, _basics.connection() /* XXX */),
        _screens(),
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

        _selector.addReadable(_pipe.readFd(), this);
        _dispatcher.add(_basics.screen()->root, this);

        while (!_finished) {
            _selector.animate();

            // Poll for X11 events that may not have shown up on the descriptor.
            _dispatcher.poll();

            if (!_exits.empty()) {
                // Purge the exited windows.
                for (auto screen : _exits) {
                    auto iter1 = _deferrals.find(screen);
                    if (iter1 != _deferrals.end()) { _deferrals.erase(iter1); }

                    auto iter2 = _screens.find(screen->getWindowId());
                    ASSERT(iter2 != _screens.end(), "");
                    _screens.erase(iter2);
                }
                _exits.clear();
            }

            // Perform the deferrals.
            for (auto screen : _deferrals) { screen->deferral(); }
            _deferrals.clear();
        }

        _dispatcher.remove(_basics.screen()->root);
        _selector.removeReadable(_pipe.readFd());

        signal(SIGCHLD, oldHandler);
    }

    void death() {
        char buf[BUFSIZ];
        auto size = sizeof buf;

        // It doesn't matter if we read more than one 'death byte' off the pipe
        // because we are going to try to reap *all* the children.
        ENFORCE_SYS(TEMP_FAILURE_RETRY(::read(_pipe.readFd(),
                                              static_cast<void *>(buf), size)) != -1, "");

        for (auto & p : _screens) {
            auto & s = p.second;
            s->tryReap();
        }
    }

    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) throw () override {
        ASSERT(fd == _pipe.readFd(), "Bad fd.");
        death();
    }

    // Screen::I_Observer implementation:

    void screenSync() throw () override {
        _dispatcher.wait(XCB_CONFIGURE_NOTIFY);      // XXX throws
    }

    void screenDefer(Screen * screen) throw () override {
        _deferrals.insert(screen);
    }

    void screenSelected(Screen * screen) throw () override {
        for (auto & p : _screens) {
            auto & s = p.second;
            if (s.get() != screen) {
                s->clearSelection();
            }
        }
    }

    void screenReaped(Screen * screen, int UNUSED(status)) throw () override {
        ASSERT(std::find(_exits.begin(), _exits.end(), screen) == _exits.end(), "");
        _exits.push_back(screen);
    }

    // I_Dispatcher::I_Observer implementation:

    void propertyNotify(xcb_property_notify_event_t * event) noexcept override {
        PRINT("Property notify");

        if (_config.x11PseudoTransparency) {
            if (event->window == _basics.screen()->root &&
                event->atom == _basics.atomXRootPixmapId())
            {
                _basics.updateRootPixmap();

                for (auto & pair : _screens) {
                    pair.second->redraw();
                }
            }
        }
    }

    // I_Creator implementation:

    void create() throw () override {
        try {
            std::unique_ptr<Screen> screen(
                new Screen(*this, _config, _selector, _deduper, _dispatcher,
                           _basics, _colorSet, _fontManager, _command));
            auto id = screen->getWindowId();
            _screens.insert(std::make_pair(id, std::move(screen)));
        }
        catch (const Screen::Error & ex) {
            PRINT("Failed to create screen: " << ex.message);
        }
    }

    void shutdown() throw () override {
        for (auto & pair : _screens) {
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

