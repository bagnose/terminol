// vi:noai:sw=4

#ifndef X_FONT_SET__HXX
#define X_FONT_SET__HXX

#include "terminol/common/support.hxx"

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

class X_FontSet : protected Uncopyable {
    Display  * _display;
    XftFont  * _normal;
    XftFont  * _bold;
    XftFont  * _italic;
    XftFont  * _italicBold;
    uint16_t   _width, _height;
    uint16_t   _ascent;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    X_FontSet(Display           * display,
              const std::string & fontName) throw (Error);

    ~X_FontSet();

    // Font accessors:

    XftFont * get(bool bold, bool italic) {
        switch ((bold ? 1 : 0) + (italic ? 2 : 0)) {
            case 0: return getNormal();
            case 1: return getBold();
            case 2: return getItalic();
            case 3: return getItalicBold();
        }
        FATAL("Unreachable");
    }

    XftFont * getNormal()     { return _normal;     }
    XftFont * getBold()       { return _bold;       }
    XftFont * getItalic()     { return _italic;     }
    XftFont * getItalicBold() { return _italicBold; }

    // Misc:

    uint16_t getWidth()  const { return _width;  }
    uint16_t getHeight() const { return _height; }
    uint16_t getAscent() const { return _ascent; }

protected:
    XftFont * load(FcPattern * pattern, bool master) throw (Error);
    void      unload(XftFont * font);
};

#endif // X_FONT_SET__HXX
