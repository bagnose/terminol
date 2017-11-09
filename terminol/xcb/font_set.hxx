// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef XCB__FONT_SET__HXX
#define XCB__FONT_SET__HXX

#include "terminol/xcb/basics.hxx"
#include "terminol/common/config.hxx"
#include "terminol/support/pattern.hxx"

#include <pango/pango-font.h>

class FontSet final : private Uncopyable {
    const Config & _config;
    Basics &       _basics;

    PangoFontDescription * _normal;
    PangoFontDescription * _bold;
    PangoFontDescription * _italic;
    PangoFontDescription * _italicBold;
    uint16_t               _width;
    uint16_t               _height;

public:
    FontSet(const Config & config, Basics & basics, int size);
    ~FontSet();

    PangoFontDescription * get(bool italic, bool bold) {
        switch ((italic ? 2 : 0) + (bold ? 1 : 0)) {
        case 0: return _normal;
        case 1: return _bold;
        case 2: return _italic;
        case 3: return _italicBold;
        }
        FATAL();
    }

    uint16_t getWidth() const { return _width; }
    uint16_t getHeight() const { return _height; }

private:
    PangoFontDescription *
         load(const std::string & family, int size, bool master, bool bold, bool italic);
    void unload(PangoFontDescription * desc);

    void measure(PangoFontDescription * desc, uint16_t & width, uint16_t & height);
};

#endif // XCB__FONT_SET__HXX
