// vi:noai:sw=4

#ifndef XCB__COLOR_SET__H
#define XCB__COLOR_SET__H

#include "terminol/xcb/basics.hxx"

#include <stdint.h>

struct Color {
    double r, g, b;
};

class ColorSet {
    static const Color COLORS16[16];

    Basics & _basics;
    Color    _cursorFgColor;
    Color    _cursorBgColor;
    Color    _indexedColors[256];
    uint32_t _backgroundPixel;

public:
    explicit ColorSet(Basics & basics);
    ~ColorSet();

    const Color & getCursorFgColor() const { return _cursorFgColor; }
    const Color & getCursorBgColor() const { return _cursorBgColor; }
    const Color & getIndexedColor(uint8_t index) const { return _indexedColors[index]; }
    uint32_t      getBackgroundPixel() const { return _backgroundPixel; }
};

#endif // XCB__COLOR_SET__H
