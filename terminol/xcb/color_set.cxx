// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/xcb/color_set.hxx"
#include "terminol/support/debug.hxx"
#include <algorithm>
#include <limits>

ColorSet::ColorSet(const Config & config, Basics & basics) : _config(config), _basics(basics) {
    _cursorFillColor  = convert(_config.cursorFillColor);
    _cursorTextColor  = convert(_config.cursorTextColor);
    _selectFgColor    = convert(_config.selectFgColor);
    _selectBgColor    = convert(_config.selectBgColor);
    _borderColor      = convert(_config.borderColor);
    _scrollBarFgColor = convert(_config.scrollbarFgColor);
    _scrollBarBgColor = convert(_config.scrollbarBgColor);

    // 0..7     normal colors
    // 8..15    bright colors
    // 16..231  6x6x6 color cube
    // 232..255 grey ramp

    uint8_t index = 0;

    for (; index != 16; ++index) { _indexedColors[index] = convert(_config.systemColors[index]); }

    for (auto r = 0; r != 6; ++r) {
        for (auto g = 0; g != 6; ++g) {
            for (auto b = 0; b != 6; ++b) {
                _indexedColors[index++] = DColor{r / 5.0, g / 5.0, b / 5.0};
            }
        }
    }

    for (auto v = 0; v != 24; ++v) {
        _indexedColors[index++] = DColor{v / 23.0, v / 23.0, v / 23.0};
    }

    ASSERT(index == 0, );

    _normalFgColor = convert(_config.normalFgColor);
    _normalBgColor = convert(_config.normalBgColor);

    //
    // Allocate the pixels.
    //

    _backgroundPixel = alloc_color(_config.normalBgColor);
    _visualBellPixel = alloc_color(_config.visualBellColor);
}

ColorSet::~ColorSet() {
    uint32_t colors[2] = {_backgroundPixel, _visualBellPixel};
    // Freeing the colour probably does nothing due to direct-color displays...
    xcb_free_colors(_basics.connection(), _basics.screen()->default_colormap, 0, 2, colors);
}

uint32_t ColorSet::alloc_color(const Color & color) {
    auto r = color.r * 0xFF;
    auto g = color.g * 0xFF;
    auto b = color.b * 0xFF;

    auto cookie = xcb_alloc_color(_basics.connection(),
                                  _basics.screen()->default_colormap,
                                  r,
                                  g,
                                  b);
    auto reply  = xcb_alloc_color_reply(_basics.connection(), cookie, nullptr);
    ENFORCE(reply, );
    auto pixel = reply->pixel;
    std::free(reply);

    return pixel;
}

DColor ColorSet::convert(const Color & color) {
    return DColor{color.r / 255.0, color.g / 255.0, color.b / 255.0};
}
