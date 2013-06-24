// vi:noai:sw=4

#include "terminol/xcb/color_set.hxx"
#include "terminol/support/debug.hxx"
#include <algorithm>
#include <limits>

ColorSet::ColorSet(const Config & config,
                   Basics       & basics) :
    _config(config),
    _basics(basics)
{
    _cursorFillColor  = convert(_config.cursorFillColor);
    _cursorTextColor  = convert(_config.cursorTextColor);
    _borderColor      = convert(_config.borderColor);
    _scrollBarFgColor = convert(_config.scrollbarFgColor);
    _scrollBarBgColor = convert(_config.scrollbarBgColor);

    // 0..7     normal colors
    // 8..15    bright colors
    // 16..231  6x6x6 color cube
    // 232..255 grey ramp

    uint8_t index = 0;

    for (; index != 16; ++index) {
        _indexedColors[index] = convert(_config.systemColors[index]);
    }

    for (auto r = 0; r != 6; ++r) {
        for (auto g = 0; g != 6; ++g) {
            for (auto b = 0; b != 6; ++b) {
                _indexedColors[index++] = XColor(r / 5.0, g / 5.0, b / 5.0);
            }
        }
    }

    for (auto v = 0; v != 24; ++v) {
        _indexedColors[index++] = XColor(v / 23.0, v / 23.0, v / 23.0);
    }

    ASSERT(index == 0, "");

    _foregroundColor = convert(_config.fgColor);
    _backgroundColor = convert(_config.bgColor);

    //
    // Allocate the background pixel.
    //

    const double MAX = std::numeric_limits<uint16_t>::max();

    uint16_t r = static_cast<uint16_t>(_backgroundColor.r * MAX + 0.5);
    uint16_t g = static_cast<uint16_t>(_backgroundColor.g * MAX + 0.5);
    uint16_t b = static_cast<uint16_t>(_backgroundColor.b * MAX + 0.5);

    xcb_alloc_color_reply_t * reply =
        xcb_alloc_color_reply(_basics.connection(),
                              xcb_alloc_color(_basics.connection(),
                                              _basics.screen()->default_colormap,
                                              r, g, b),
                              nullptr);
    ENFORCE(reply, "");
    _backgroundPixel = reply->pixel;
    std::free(reply);
}

ColorSet::~ColorSet() {
    xcb_free_colors(_basics.connection(),
                    _basics.screen()->default_colormap,
                    0,         // plane_mask - WTF?
                    1,
                    &_backgroundPixel);
}

XColor ColorSet::convert(const Color & color) {
    return XColor(color.r / 255.0,
                  color.g / 255.0,
                  color.b / 255.0);
}
