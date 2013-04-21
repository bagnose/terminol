// vi:noai:sw=4

#ifndef COLOR_SET__H
#define COLOR_SET__H

#include <stdint.h>

struct Color {
    double r, g, b;
};

class X_ColorSet {
    static const Color COLORS16[16];

    Color _indexedColors[256];
    Color _cursorColor;

public:
    X_ColorSet();

    const Color & getIndexedColor(uint8_t index) const { return _indexedColors[index]; }
    const Color & getCursorColor() const { return _cursorColor; }
};

#endif // COLOR_SET__H
