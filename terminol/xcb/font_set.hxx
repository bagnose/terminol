// vi:noai:sw=4

#ifndef XCB__FONT_SET__HXX
#define XCB__FONT_SET__HXX

#include <cairo-ft.h>

#include "terminol/common/support.hxx"
#include "terminol/common/config.hxx"

class FontSet : protected Uncopyable {
    const Config        & _config;
    cairo_scaled_font_t * _normal;
    cairo_scaled_font_t * _bold;
    cairo_scaled_font_t * _italic;
    cairo_scaled_font_t * _italicBold;
    uint16_t              _width, _height;
    uint16_t              _ascent;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    explicit FontSet(const Config & config) throw (Error);
    ~FontSet();

    // Font accessors:

    cairo_scaled_font_t * get(bool bold, bool italic) {
        switch ((bold ? 1 : 0) + (italic ? 2 : 0)) {
            case 0: return getNormal();
            //case 1: return getBold();
            case 1: return getNormal();
            case 2: return getItalic();
            //case 3: return getItalicBold();
            case 3: return getItalic();
        }
        FATAL("Unreachable");
    }

    cairo_scaled_font_t * getNormal()     { return _normal;     }
    cairo_scaled_font_t * getBold()       { return _bold;       }
    cairo_scaled_font_t * getItalic()     { return _italic;     }
    cairo_scaled_font_t * getItalicBold() { return _italicBold; }

    // Misc:

    uint16_t getWidth()  const { return _width;  }
    uint16_t getHeight() const { return _height; }
    uint16_t getAscent() const { return _ascent; }

protected:
    cairo_scaled_font_t * load(FcPattern * pattern, bool master) throw (Error);
    void                  unload(cairo_scaled_font_t * font);
};

#endif // XCB__FONT_SET__HXX
