// vi:noai:sw=4

#ifndef XCB__COLOR_SET__H
#define XCB__COLOR_SET__H

#include "terminol/xcb/basics.hxx"
#include "terminol/common/config.hxx"

#include <stdint.h>

struct Color {
    double r, g, b;

    // XXX DODGY
    Color & operator = (const Config::Color & color) {
        r = color.r;
        g = color.g;
        b = color.b;
        return *this;
    }
};

class ColorSet {
    //static const Color COLORS16[16];

    const Config & _config;
    Basics       & _basics;
    Color          _cursorFgColor;
    Color          _cursorBgColor;
    Color          _borderColor;
    Color          _scrollBarFgColor;
    Color          _scrollBarBgColor;
    Color          _indexedColors[256];
    uint32_t       _backgroundPixel;

public:
    explicit ColorSet(const Config & config,
                      Basics       & basics);
    ~ColorSet();

    const Color & getCursorFgColor()    const { return _cursorFgColor; }
    const Color & getCursorBgColor()    const { return _cursorBgColor; }
    const Color & getBorderColor()      const { return _borderColor; }
    const Color & getScrollBarFgColor() const { return _scrollBarFgColor; }
    const Color & getScrollBarBgColor() const { return _scrollBarBgColor; }
    const Color & getIndexedColor(uint8_t index) const { return _indexedColors[index]; }
    //const Color & getBackgroundColor()  const { return _indexedColors[0]; }
    uint32_t      getBackgroundPixel()  const { return _backgroundPixel; }
};

#endif // XCB__COLOR_SET__H
