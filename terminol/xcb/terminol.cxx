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
#include "terminol/support/async_invoker.hxx"
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

class EventLoop final :
    protected I_Selector::I_ReadHandler,
    protected Screen::I_Observer,
    protected I_Dispatcher::I_Observer,
    protected Uncopyable
{
    const Config     & _config;
    Selector           _selector;
    Pipe               _pipe;
    SimpleDeduper      _deduper;
    AsyncInvoker       _asyncInvoker; // Must be declared after anything indirectly used by it.
    Basics             _basics;
    ColorSet           _colorSet;
    FontManager        _fontManager;
    Dispatcher         _dispatcher;
    Screen             _screen;
    bool               _deferral = false;
    bool               _exited   = false;

    static EventLoop * _singleton;

public:
    EventLoop(const Config       & config,
              const Tty::Command & command) :
        _config(config),
        _colorSet(config, _basics),
        _fontManager(config, _basics),
        _dispatcher(_selector, _basics.connection() /* XXX */),
        _screen(*this,
                config,
                _selector,
                _deduper,
                _asyncInvoker,
                _dispatcher,
                _basics,
                _colorSet,
                _fontManager,
                command)
    {
        ENFORCE(std::exchange(_singleton, this) == nullptr, "");

        if (_config.x11PseudoTransparency) {
            uint32_t mask = XCB_EVENT_MASK_PROPERTY_CHANGE;
            xcb_change_window_attributes(_basics.connection(),
                                         _basics.screen()->root,
                                         XCB_CW_EVENT_MASK,
                                         &mask);
        }

        loop();
    }

    ~EventLoop() {
        ENFORCE(std::exchange(_singleton, nullptr) == this, "");
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
        auto oldHandler = ::signal(SIGCHLD, &staticSignalHandler);

        _selector.addReadable(_pipe.readFd(), this);
        _dispatcher.add(_basics.screen()->root, this);

        for (;;) {
            _selector.animate();

            // Poll for X11 events that may not have shown up on the descriptor.
            _dispatcher.poll();

            if (_exited)   { break; }
            if (_deferral) { _screen.deferral(); _deferral = false; }
        }

        _dispatcher.remove(_basics.screen()->root);
        _selector.removeReadable(_pipe.readFd());

        ::signal(SIGCHLD, oldHandler);
    }

    void death() {
        std::array<char, BUFSIZ> buf;
        THROW_IF_SYSCALL_FAILS(::read(_pipe.readFd(), static_cast<void *>(buf.data()), buf.size()), "read()");
        _screen.tryReap(); // XXX We should assert that this succeeds. Why bother with screenReaped?
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

    void propertyNotify(xcb_property_notify_event_t * event) override {
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

int main(int argc, char * argv[]) try {
    Config config;

    parseConfig(config);

    CmdLine cmdLine(makeHelp(argv[0]), VERSION, "--execute");
    cmdLine.add(std::make_unique<StringHandler>(config.fontName), '\0', "font-name");
    cmdLine.add(std::make_unique<IntHandler>(config.fontSize),    '\0', "font-size");
    cmdLine.add(std::make_unique<BoolHandler>(config.traceTty),   '\0', "trace");
    cmdLine.add(std::make_unique<BoolHandler>(config.syncTty),    '\0', "sync");
    cmdLine.add(std::make_unique<BoolHandler>(config.traditionalWrapping), '\0', "traditional-wrapping");
    cmdLine.add(std::make_unique<StringHandler>(config.termName), '\0', "term-name");
    cmdLine.add(std::make_unique<MiscHandler>([&](const std::string & name) {
                                                  config.setColorScheme(name);
                                              }), '\0', "color-scheme");

    auto command = cmdLine.parse(argc, const_cast<const char **>(argv));
    EventLoop eventLoop(config, command);

    return 0;
}
catch (const UserError & ex) {
    std::cerr << ex.message() << std::endl;
    return 1;
}
