// vi:noai:sw=4

#ifndef XCB__FONT_SET__HXX
#define XCB__FONT_SET__HXX

#include "terminol/common/config.hxx"
#include "terminol/support/pattern.hxx"

#include <cairo-ft.h>

class FontSet : protected Uncopyable {
    const Config        & _config;
    bool                  _firstLoad;
    cairo_scaled_font_t * _normal;
    cairo_scaled_font_t * _bold;
    cairo_scaled_font_t * _italic;
    cairo_scaled_font_t * _italicBold;
    uint16_t              _width;
    uint16_t              _height;
    uint16_t              _ascent;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    explicit FontSet(const Config & config) throw (Error);
    ~FontSet();

    // Font accessors:

    cairo_scaled_font_t * get(bool italic, bool bold) {
        switch ((italic ? 2 : 0) + (bold ? 1 : 0)) {
            case 0: return getNormal();
            case 1: return getBold();
            case 2: return getItalic();
            case 3: return getItalicBold();
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
    cairo_scaled_font_t * load(FcPattern * pattern) throw (Error);
    void                  unload(cairo_scaled_font_t * font);
};

#endif // XCB__FONT_SET__HXX
