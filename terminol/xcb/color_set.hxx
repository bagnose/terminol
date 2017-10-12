// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef XCB__COLOR_SET__H
#define XCB__COLOR_SET__H

#include "terminol/xcb/basics.hxx"
#include "terminol/common/config.hxx"

#include <cstdint>

struct DColor {
    double r = 0.0;
    double g = 0.0;
    double b = 0.0;
};

//
//
//

class ColorSet {
    const Config & _config;
    Basics       & _basics;
    DColor         _cursorFgColor;
    DColor         _cursorBgColor;
    DColor         _selectFgColor;
    DColor         _selectBgColor;
    DColor         _cursorFillColor;
    DColor         _cursorTextColor;
    DColor         _borderColor;
    DColor         _scrollBarFgColor;
    DColor         _scrollBarBgColor;
    DColor         _indexedColors[256];
    DColor         _normalFgColor;
    DColor         _normalBgColor;
    uint32_t       _backgroundPixel;
    uint32_t       _visualBellPixel;

public:
    ColorSet(const Config & config, Basics & basics);
    ~ColorSet();

    const DColor & getCursorFillColor()  const { return _cursorFillColor; }
    const DColor & getCursorTextColor()  const { return _cursorTextColor; }
    const DColor & getSelectFgColor()    const { return _selectFgColor; }
    const DColor & getSelectBgColor()    const { return _selectBgColor; }
    const DColor & getBorderColor()      const { return _borderColor; }
    const DColor & getScrollBarFgColor() const { return _scrollBarFgColor; }
    const DColor & getScrollBarBgColor() const { return _scrollBarBgColor; }
    const DColor & getIndexedColor(uint8_t index) const { return _indexedColors[index]; }
    const DColor & getNormalFgColor()    const { return _normalFgColor; }
    const DColor & getNormalBgColor()    const { return _normalBgColor; }
    uint32_t       getBackgroundPixel()  const { return _backgroundPixel; }
    uint32_t       getVisualBellPixel()  const { return _visualBellPixel; }

private:
    uint32_t alloc_color(const Color & color);

    static DColor convert(const Color & color);
};

#endif // XCB__COLOR_SET__H
