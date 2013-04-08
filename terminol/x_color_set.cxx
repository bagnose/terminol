// vi:noai:sw=4

#include "terminol/x_color_set.hxx"

namespace {

const char * names[] = {
    // 8 normal colors
    "black",
    "red3",
    "green3",
    "yellow3",
    "blue2",
    "magenta3",
    "cyan3",
    "gray90",
    // 8 bright colors
    "gray50",
    "red",
    "green",
    "yellow",
    "#5c5cff",
    "magenta",
    "cyan",
    "white"
};

uint16_t sixd_to_16bit(int x) {
    return x == 0 ? 0 : 0x3737 + 0x2828 * x;
}

} // namespace {anonymous}

X_ColorSet::X_ColorSet(Display  * display,
                       Visual   * visual,
                       Colormap  colormap) :
    _display(display),
    _visual(visual),
    _colormap(colormap),
    _indexedColors(),
    _cursorColor()
{
    // 0..7     normal colors
    // 8..15    bright colors
    // 16..231  6x6x6 color cube
    // 232..255 grey ramp

    uint8_t index = 0;

    for (auto i = 0; i != 16; ++i) {
        const char * n = names[i];
        XftColorAllocName(_display, _visual, _colormap, n, &_indexedColors[i]);
        ++index;
    }

    for (auto r = 0; r != 6; ++r) {
        for (auto g = 0; g != 6; ++g) {
            for (auto b = 0; b != 6; ++b) {
                XRenderColor  xrColor;
                xrColor.red   = sixd_to_16bit(r);
                xrColor.green = sixd_to_16bit(g);
                xrColor.blue  = sixd_to_16bit(b);
                xrColor.alpha = 0xffff;
                XftColorAllocValue(_display,
                                   _visual,
                                   _colormap,
                                   &xrColor,
                                   &_indexedColors[index]);
            }
        }
        ++index;
    }

    for (auto v = 0; v != 24; ++v) {
        XRenderColor  xrColor;
        xrColor.red = xrColor.green = xrColor.blue = 0x0808 + 0x0a0a * v;
        xrColor.alpha = 0xffff;
        XftColorAllocValue(_display,
                           _visual,
                           _colormap,
                           &xrColor,
                           &_indexedColors[index]);
        ++index;
    }

    //
    //
    //

    {
        XRenderColor  xrColor;
        xrColor.red   = 0xffff;
        xrColor.green = 0x0000;
        xrColor.blue  = 0xffff;
        xrColor.alpha = 0xffff;
        XftColorAllocValue(_display,
                           _visual,
                           _colormap,
                           &xrColor,
                           &_cursorColor);
    }
}

X_ColorSet::~X_ColorSet() {
    for (auto xftColor : _indexedColors) {
        XftColorFree(_display, _visual, _colormap, &xftColor);
    }
}
