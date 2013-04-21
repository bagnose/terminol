// vi:noai:sw=4

#include "terminol/xcb/color_set.hxx"
#include "terminol/common/support.hxx"

#include <algorithm>

const Color X_ColorSet::COLORS16[16] = {
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

X_ColorSet::X_ColorSet() {
    // 0..7     normal colors
    // 8..15    bright colors
    // 16..231  6x6x6 color cube
    // 232..255 grey ramp

    std::copy(COLORS16, COLORS16 + 16, _indexedColors);

    uint8_t index = 16;

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

    ASSERT(index == 0, "Should have wrapped to zero.");

    _cursorColor = { 1.0, 0.0, 1.0 };
}
