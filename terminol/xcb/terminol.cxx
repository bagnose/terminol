// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/xcb/screen.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/common.hxx"
#include "terminol/xcb/dispatcher.hxx"
#include "terminol/common/simple_deduper.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/async_destroyer.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pipe.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/support/cmdline.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_event.h>
#include <xcb/xcb_aux.h>

#include <unistd.h>
#include <sys/select.h>

class EventLoop :
    protected I_Selector::I_ReadHandler,
    protected Screen::I_Observer,
    protected I_Dispatcher::I_Observer,
    protected Uncopyable
{
    const Config     & _config;
    Selector           _selector;
    Pipe               _pipe;
    SimpleDeduper      _deduper;
    AsyncDestroyer     _destroyer;      // Must be declared after anything indirectly used by it.
    Basics             _basics;
    ColorSet           _colorSet;
    FontManager        _fontManager;
    Dispatcher         _dispatcher;
    Screen             _screen;
    bool               _deferral;
    bool               _exited;

    static EventLoop * _singleton;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    EventLoop(const Config       & config,
              const Tty::Command & command) :
        _config(config),
        _selector(),
        _pipe(),
        _deduper(),
        _destroyer(),
        _basics(),
        _colorSet(config, _basics),
        _fontManager(config, _basics),
        _dispatcher(_selector, _basics.connection() /* XXX */),
        _screen(*this,
                config,
                _selector,
                _deduper,
                _destroyer,
                _dispatcher,
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

    void loop() {
        auto oldHandler = signal(SIGCHLD, &staticSignalHandler);

        _selector.addReadable(_pipe.readFd(), this);
        _dispatcher.add(_basics.screen()->root, this);

        for (;;) {
            _selector.animate();

            // Poll for X11 events that may not have shown up on the descriptor.
            _dispatcher.poll();     // XXX throws

            if (_exited)   { break; }
            if (_deferral) { _screen.deferral(); _deferral = false; }
        }

        _dispatcher.remove(_basics.screen()->root);
        _selector.removeReadable(_pipe.readFd());

        signal(SIGCHLD, oldHandler);
    }

    void death() {
        char buf[BUFSIZ];
        auto size = sizeof buf;

        ENFORCE_SYS(TEMP_FAILURE_RETRY(::read(_pipe.readFd(),
                                              static_cast<void *>(buf), size)) != -1, "");

        _screen.tryReap();          // XXX We should assert that this success. Why bother with screenReaped?
    }

    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) override {
        ASSERT(fd == _pipe.readFd(), "Bad fd.");
        death();
    }

    // Screen::I_Observer implementation:

    void screenSync() override {
        _dispatcher.wait(XCB_CONFIGURE_NOTIFY);
    }

    void screenDefer(Screen * screen) override {
        ASSERT(screen == &_screen, "");
        _deferral = true;
    }

    void screenSelected(Screen * UNUSED(screen)) override {
        // Nothing to do.
    }

    void screenReaped(Screen * screen, int UNUSED(status)) override {
        ASSERT(screen == &_screen, "");
        _exited = true;
    }

    // I_Dispatcher::I_Observer implementation:

    void propertyNotify(xcb_property_notify_event_t * event) noexcept override {
        if (_config.x11PseudoTransparency) {
            if (event->window == _basics.screen()->root &&
                event->atom == _basics.atomXRootPixmapId())
            {
                _basics.updateRootPixmap();
                _screen.redraw();
            }
        }
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

    try {
        parseConfig(config);
    }
    catch (const ParseError & error) {
        FATAL(error.message);
    }

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
                                catch (const ParseError & error) {
                                    throw CmdLine::Handler::Error(error.message);
                                }
                                }), '\0', "color-scheme");

    try {
        auto command = cmdLine.parse(argc, const_cast<const char **>(argv));
        EventLoop eventLoop(config, command);
    }
    catch (const EventLoop::Error & error) {
        FATAL(error.message);
    }
    catch (const Dispatcher::Error & error) {
        FATAL(error.message);
    }
    catch (const Screen::Error & error) {
        FATAL(error.message);
    }
    catch (const Basics::Error & error) {
        FATAL(error.message);
    }
    catch (const CmdLine::Error & error) {
        FATAL(error.message);
    }

    return 0;
}
