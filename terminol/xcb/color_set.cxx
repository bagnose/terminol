// vi:noai:sw=4

#include "terminol/xcb/color_set.hxx"
#include "terminol/support/debug.hxx"

#include <algorithm>
#include <limits>

#if 0
#if 0
const Color ColorSet::COLORS16[16] = {
    // normal
    { 0.0,  0.0,  0.0  }, // black
    { 0.66, 0.0,  0.0  }, // red
    { 0.0,  0.66, 0.0  }, // green
    { 0.66, 0.66, 0.0  }, // orange
    { 0.0,  0.0,  0.66 }, // blue
    { 0.66, 0.0,  0.66 }, // magenta
    { 0.0,  0.66, 0.66 }, // cyan
    { 0.66, 0.66, 0.66 }, // light grey
    // bright
    { 0.33, 0.33, 0.33 }, // dark grey
    { 1.0,  0.33, 0.33 }, // high red
    { 0.33, 1.0,  0.33 }, // high green
    { 1.0,  1.0,  0.33 }, // high yellow
    { 0.33, 0.33, 1.0  }, // high blue
    { 1.0,  0.33, 1.0  }, // high magenta
    { 0.33, 1.0,  1.0  }, // high cyan
    { 1.0,  1.0,  1.0  }  // white
};
#else
const Color ColorSet::COLORS16[16] = {
    // normal
    { 0.0,  0.0,  0.08 }, // black
    { 0.66, 0.0,  0.0  }, // red
    { 0.0,  0.66, 0.0  }, // green
    { 0.66, 0.66, 0.0  }, // orange
    { 0.33, 0.33, 0.90 }, // blue
    { 0.66, 0.0,  0.66 }, // magenta
    { 0.0,  0.66, 0.66 }, // cyan
    { 0.66, 0.66, 0.66 }, // light grey
    // bright
    { 0.33, 0.33, 0.33 }, // dark grey
    { 1.0,  0.33, 0.33 }, // high red
    { 0.33, 1.0,  0.33 }, // high green
    { 1.0,  1.0,  0.33 }, // high yellow
    { 0.66, 0.66, 1.0  }, // high blue
    { 1.0,  0.33, 1.0  }, // high magenta
    { 0.33, 1.0,  1.0  }, // high cyan
    { 1.0,  1.0,  1.0  }  // white
};
#endif
#endif

ColorSet::ColorSet(const Config & config,
                   Basics       & basics) :
    _config(config),
    _basics(basics)
{
    _cursorFgColor    = { 0.0, 0.0,  0.0 };
    _cursorBgColor    = { 1.0, 0.25, 1.0 };
#if 0
    _borderColor      = _config.getSystemColor(0);
#else
    _borderColor      = _config.getSystemColor(8);
#endif
    _scrollBarFgColor = { 0.5, 0.5,  0.5 };
    _scrollBarBgColor = _borderColor;

    // 0..7     normal colors
    // 8..15    bright colors
    // 16..231  6x6x6 color cube
    // 232..255 grey ramp
    // 256      foreground
    // 257      background

    uint16_t index = 0;

    for (; index != 16; ++index) {
        _indexedColors[index] = _config.getSystemColor(index);
    }

    for (auto r = 0; r != 6; ++r) {
        for (auto g = 0; g != 6; ++g) {
            for (auto b = 0; b != 6; ++b) {
                _indexedColors[index++] = { r / 5.0, g / 5.0, b / 5.0 };
            }
        }
    }

    for (auto v = 0; v != 24; ++v) {
        _indexedColors[index++] = { v / 23.0, v / 23.0, v / 23.0 };
    }

    _indexedColors[index++] = _config.getFgColor();
    _indexedColors[index++] = _config.getBgColor();

    ASSERT(index == 258, "");

    //
    // Allocate the background pixel.
    //

    Color background = _config.getBgColor();

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
