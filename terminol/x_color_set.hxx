// vi:noai:sw=4

#ifndef X_COLOR_SET__HXX
#define X_COLOR_SET__HXX

#include "terminol/common.hxx"

#include <vector>

#include <X11/Xlib.h>
#include <X11/Xft/Xft.h>

class X_ColorSet : protected Uncopyable {
    Display  * _display;
    Visual   * _visual;
    Colormap   _colormap;
    XftColor   _indexedColors[256];
    XftColor   _cursorColor;

public:
    X_ColorSet(Display  * display,
               Visual   * visual,
               Colormap   colormap);

    ~X_ColorSet();

    const XftColor * getIndexedColor(uint8_t index) const {
        return &_indexedColors[index];
    }

    const XftColor * getCursorColor() const { return &_cursorColor; }
};

#endif // X_COLOR_SET__HXX
