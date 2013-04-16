// vi:noai:sw=4

#ifndef X_BASICS__HXX
#define X_BASICS__HXX

#include "terminol/common.hxx"

#include <X11/Xlib.h>
#include <sstream>

class X_Basics {
    Display  * _display;
    Screen   * _screen;
    Visual   * _visual;
    Colormap   _colormap;
    Window     _root;
    XIM        _xim;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    X_Basics() throw (Error) {
        _display  = XOpenDisplay(nullptr);
        if (!_display) throw Error("Couldn't open display.");

        try {
            _screen   = XDefaultScreenOfDisplay(_display);
            ASSERT(_screen, "");
            _visual   = XDefaultVisualOfScreen(_screen);
            ASSERT(_visual, "");
            _colormap = XDefaultColormapOfScreen(_screen);
            _root     = XRootWindowOfScreen(_screen);

            _xim = XOpenIM(_display, nullptr, nullptr, nullptr);
            if (!_xim) {
                XSetLocaleModifiers("@im=local");
                _xim = XOpenIM(_display, nullptr, nullptr, nullptr);
                if (!_xim) {
                    XSetLocaleModifiers("@im=");
                    _xim = XOpenIM(_display, nullptr, nullptr, nullptr);
                    // Last chance.
                    if (!_xim) throw Error("Couldn't open Input Method.");
                }
            }
        }
        catch (...) {
            XCloseDisplay(_display);
            throw;
        }
    }

    ~X_Basics() {
        if (_display) {
            XCloseIM(_xim);
            XCloseDisplay(_display);
        }
    }

    Display * display()  { return _display;  }
    Screen  * screen()   { return _screen;   }
    Visual  * visual()   { return _visual;   }
    Colormap  colormap() { return _colormap; }
    Window    root()     { return _root;     }
    XIM       xim()      { return _xim;      }

    void connectionLost() { _display = nullptr; }

    //
    //
    //

    static std::string stateToStr(uint8_t state) {
        // Run xmodmap without any arguments to discover these.
        std::ostringstream maskStr;
        if (state & ShiftMask)   maskStr << "|SHIFT";
        if (state & LockMask)    maskStr << "|LOCK";
        if (state & ControlMask) maskStr << "|CTRL";
        if (state & Mod1Mask)    maskStr << "|ALT";
        if (state & Mod2Mask)    maskStr << "|NUM";
        if (state & Mod3Mask)    maskStr << "|MOD3";
        if (state & Mod4Mask)    maskStr << "|WIN";
        if (state & Mod5Mask)    maskStr << "|MOD5";
        std::string result = maskStr.str();

        // Strip the leading '|', if present.
        return result.empty() ? result : result.substr(1);
    }
};

#endif // X_BASICS__HXX
