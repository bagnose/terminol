// vi:noai:sw=4

#ifndef XCB__COLOR_SET__H
#define XCB__COLOR_SET__H

#include "terminol/xcb/basics.hxx"
#include "terminol/common/config.hxx"

#include <stdint.h>

struct ColoR {          // FIXME rename
    ColoR() : r(0.0), g(0.0), b(0.0) {}
    ColoR(double r_, double g_, double b_) : r(r_), g(g_), b(b_) {}

    double r, g, b;
};

class ColorSet {
    const Config & _config;
    Basics       & _basics;
    ColoR         _cursorFgColor;
    ColoR         _cursorBgColor;
    ColoR         _borderColor;
    ColoR         _scrollBarFgColor;
    ColoR         _scrollBarBgColor;
    ColoR         _indexedColors[258];
    uint32_t       _backgroundPixel;
    ColoR         _cursorFillColor;
    ColoR         _cursorTextColor;

public:
    explicit ColorSet(const Config & config,
                      Basics       & basics);
    ~ColorSet();

    const ColoR & getCursorFillColor()  const { return _cursorFillColor; }
    const ColoR & getCursorTextColor()  const { return _cursorTextColor; }
    const ColoR & getBorderColor()      const { return _borderColor; }
    const ColoR & getScrollBarFgColor() const { return _scrollBarFgColor; }
    const ColoR & getScrollBarBgColor() const { return _scrollBarBgColor; }
    const ColoR & getIndexedColor(uint16_t index) const {
        ASSERT(index < 258, "Index out of range: " << index);
        return _indexedColors[index];
    }
    const ColoR & getForegroundColor()  const { return _indexedColors[256]; }
    const ColoR & getBackgroundColor()  const { return _indexedColors[257]; }
    uint32_t      getBackgroundPixel()  const { return _backgroundPixel; }

    static ColoR convert(const Color & color);
};

#endif // XCB__COLOR_SET__H
