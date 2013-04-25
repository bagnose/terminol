// vi:noai:sw=4

#ifndef XCB__COLOR_SET__H
#define XCB__COLOR_SET__H

#include <stdint.h>

struct Color {
    double r, g, b;
};

class ColorSet {
    static const Color COLORS16[16];

    Color _indexedColors[256];
    Color _cursorColor;

public:
    ColorSet();

    const Color & getIndexedColor(uint8_t index) const { return _indexedColors[index]; }
    const Color & getCursorColor() const { return _cursorColor; }
};

#endif // XCB__COLOR_SET__H
