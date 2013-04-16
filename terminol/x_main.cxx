// vi:noai:sw=4

#include "terminol/common.hxx"
#include "terminol/x_basics.hxx"
#include "terminol/x_window.hxx"
#include "terminol/x_color_set.hxx"
#include "terminol/x_key_map.hxx"
#include "terminol/x_font_set.hxx"

#include <string>

#include <unistd.h>
#include <X11/Xlib.h>
#include <fontconfig/fontconfig.h>

class EventLoop : protected Uncopyable {
    X_Basics   _basics;
    X_ColorSet _colorSet;
    X_KeyMap   _keyMap;
    X_FontSet  _fontSet;
    X_Window   _window;

public:
    EventLoop(const std::string           & fontName,
              const std::string           & term,
              const Interlocutor::Command & command)
        throw (X_Basics::Error, X_ColorSet::Error, X_FontSet::Error, X_Window::Error) :
        _basics(),
        _colorSet(_basics.display(), _basics.visual(), _basics.colormap()),
        _keyMap(),
        _fontSet(_basics.display(), fontName),
        _window(_basics.display(), _basics.root(), _basics.screen(), _basics.xim(),
                _colorSet, _keyMap, _fontSet, term, command)
    {
        loop();
    }

protected:
    void loop() {
        while (_window.isOpen()) {
            int fdMax = 0;
            fd_set readFds, writeFds;
            FD_ZERO(&readFds); FD_ZERO(&writeFds);

            FD_SET(XConnectionNumber(_basics.display()), &readFds);
            fdMax = std::max(fdMax, XConnectionNumber(_basics.display()));

            FD_SET(_window.getFd(), &readFds);
            fdMax = std::max(fdMax, _window.getFd());

            bool selectOnWrite = _window.isWritePending();
            if (selectOnWrite) {
                FD_SET(_window.getFd(), &writeFds);
                fdMax = std::max(fdMax, _window.getFd());
            }

            //PRINT("Calling select");
            ENFORCE_SYS(TEMP_FAILURE_RETRY(
                ::select(fdMax + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");
            //PRINT("SELECT returned");

            // Handle _one_ I/O.

            if (selectOnWrite && FD_ISSET(_window.getFd(), &writeFds)) {
                //PRINT("window write event");
                _window.write();
            }
            else if (FD_ISSET(XConnectionNumber(_basics.display()), &readFds)) {
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

    void xevent() {
        while (XPending(_basics.display()) != 0) {
            XEvent event;
            XNextEvent(_basics.display(), &event);
            if (XFilterEvent(&event, None)) {
                PRINT("Filtered!");
                continue;
            }

            switch (event.type) {
                case KeyPress:
                    _window.keyPress(event.xkey);
                    break;
                case KeyRelease:
                    _window.keyRelease(event.xkey);
                    break;
                case ButtonPress:
                    _window.buttonPress(event.xbutton);
                    break;
                case ButtonRelease:
                    _window.buttonRelease(event.xbutton);
                    break;
                case MotionNotify:
                    _window.motionNotify(event.xmotion);
                    break;
                case MapNotify:
                    _window.mapNotify(event.xmap);
                    break;
                case UnmapNotify:
                    _window.unmapNotify(event.xunmap);
                    break;
                case ReparentNotify:
                    _window.reparentNotify(event.xreparent);
                    break;
                case Expose:
                    _window.expose(event.xexpose);
                    break;
                case ConfigureNotify:
                    _window.configureNotify(event.xconfigure);
                    break;
                case EnterNotify:
                    _window.enterNotify(event.xcrossing);
                    break;
                case LeaveNotify:
                    _window.leaveNotify(event.xcrossing);
                    break;
                case FocusIn:
                    _window.focusIn(event.xfocus);
                    break;
                case FocusOut:
                    _window.focusOut(event.xfocus);
                    break;
                case VisibilityNotify:
                    _window.visibilityNotify(event.xvisibility);
                    break;
                case DestroyNotify:
                    _window.destroyNotify(event.xdestroywindow);
                    break;
                default:
                    PRINT("Unhandled event: " << event.type);
                    break;
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
    // Steps:
    // - parse cmd-line args
    // - convert font string to fc-pattern
    // - open display
    // - load font
    // - open socket
    // - event loop
    // -   socket read
    // -   socket write
    // -   window
    // -   master read
    // -   master write

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

#if 0
    if (!geometryStr.empty()) {
        int          x, y;
        unsigned int w, h;
        int bits = XParseGeometry(geometryStr.c_str(), &x, &y, &w, &h);

        if (bits & XValue) {}
        if (bits & YValue) {}
        if (bits & XNegative) {}
        if (bits & YNegative) {}
        if (bits & WidthValue) {}
        if (bits & HeightValue) {}
    }
#endif

    FcInit();

    try {
        // RAII.
        EventLoop(fontName, term, command);
    }
    catch (X_Window::Error & ex) {
        FATAL(ex.message);
    }
    catch (X_FontSet::Error & ex) {
        FATAL(ex.message);
    }
    catch (X_ColorSet::Error & ex) {
        FATAL(ex.message);
    }
    catch (X_Basics::Error & ex) {
        FATAL(ex.message);
    }

    FcFini();

    return 0;
}
