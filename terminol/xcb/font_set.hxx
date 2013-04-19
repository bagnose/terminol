// vi:noai:sw=4

#ifndef FONT_SET__HXX
#define FONT_SET__HXX

#include <cairo-ft.h>

#include "terminol/common/support.hxx"

class X_FontSet : protected Uncopyable {
    cairo_font_face_t * _normal;
    cairo_font_face_t * _bold;
    cairo_font_face_t * _italic;
    cairo_font_face_t * _italicBold;
    uint16_t            _width, _height;
    uint16_t            _ascent;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    explicit X_FontSet(const std::string & fontName) throw (Error);
    ~X_FontSet();

    // Font accessors:

    cairo_font_face_t * get(bool bold, bool italic) {
        switch ((bold ? 1 : 0) + (italic ? 2 : 0)) {
            case 0: return getNormal();
            case 1: return getBold();
            case 2: return getItalic();
            case 3: return getItalicBold();
        }
        FATAL("Unreachable");
    }

    cairo_font_face_t * getNormal()     { return _normal;     }
    cairo_font_face_t * getBold()       { return _bold;       }
    cairo_font_face_t * getItalic()     { return _italic;     }
    cairo_font_face_t * getItalicBold() { return _italicBold; }

    // Misc:

    uint16_t getWidth()  const { return _width;  }
    uint16_t getHeight() const { return _height; }
    uint16_t getAscent() const { return _ascent; }

protected:
    cairo_font_face_t * load(FcPattern * pattern, bool master) throw (Error);
    void                unload(cairo_font_face_t * font);
};

#endif // FONT_SET__HXX
