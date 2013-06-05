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
    _cursorFillColor  = convert(_config.getCursorFillColor());
    _cursorTextColor  = convert(_config.getCursorTextColor());
    _borderColor      = convert(_config.getBorderColor());
    _scrollBarFgColor = convert(_config.getScrollbarFgColor());
    _scrollBarBgColor = convert(_config.getScrollbarBgColor());

    // 0..7     normal colors
    // 8..15    bright colors
    // 16..231  6x6x6 color cube
    // 232..255 grey ramp
    // 256      foreground
    // 257      background

    uint16_t index = 0;

    for (; index != 16; ++index) {
        _indexedColors[index] = convert(_config.getSystemColor(index));
    }

    for (auto r = 0; r != 6; ++r) {
        for (auto g = 0; g != 6; ++g) {
            for (auto b = 0; b != 6; ++b) {
                _indexedColors[index++] = ColoR(r / 5.0, g / 5.0, b / 5.0);
            }
        }
    }

    for (auto v = 0; v != 24; ++v) {
        _indexedColors[index++] = ColoR(v / 23.0, v / 23.0, v / 23.0);
    }

    _indexedColors[index++] = convert(_config.getFgColor());
    _indexedColors[index++] = convert(_config.getBgColor());

    ASSERT(index == 258, "");

    //
    // Allocate the background pixel.
    //

    ColoR background = convert(_config.getBgColor());

    const double MAX = std::numeric_limits<uint16_t>::max();

    uint16_t r = static_cast<uint16_t>(background.r * MAX + 0.5);
    uint16_t g = static_cast<uint16_t>(background.g * MAX + 0.5);
    uint16_t b = static_cast<uint16_t>(background.b * MAX + 0.5);

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

ColoR ColorSet::convert(const Color & color) {
    return ColoR(color.r / 255.0,
                 color.g / 255.0,
                 color.b / 255.0);
}
