// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__CONFIG__HXX
#define COMMON__CONFIG__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/common/data_types.hxx"
#include "terminol/common/bindings.hxx"

struct Config {
    // titleUpdateStrategy: replace, append, prepend, ignore

    std::string fontName;
    int         fontSize;
    std::string termName;
    bool        scrollWithHistory;
    bool        scrollOnTtyOutput;
    bool        scrollOnTtyKeyPress;
    bool        scrollOnResize;
    bool        scrollOnPaste;
    std::string title;
    std::string icon;
    std::string chdir;
    size_t      scrollBackHistory;
    bool        unlimitedScrollBack;
    int         framesPerSecond;
    bool        traditionalWrapping;
    // Debugging support:
    bool        traceTty;
    bool        syncTty;
    //
    int16_t     initialX;
    int16_t     initialY;
    uint16_t    initialRows;
    uint16_t    initialCols;

    Color       normalFgColor;
    Color       normalBgColor;
    Color       systemColors[16];

    bool        customSelectBgColor;
    Color       selectBgColor;
    bool        customSelectFgColor;
    Color       selectFgColor;

    bool        customCursorFillColor;
    Color       cursorFillColor;
    bool        customCursorTextColor;
    Color       cursorTextColor;

    Color       scrollbarFgColor;
    Color       scrollbarBgColor;
    bool        scrollbarVisible;
    int         scrollbarWidth;

    Color       borderColor;
    int         borderThickness;
    uint32_t    doubleClickTimeout;

    std::string socketPath;
    bool        serverFork;

    Bindings    bindings;

    std::string cutChars;

    bool        autoHideCursor;

    Color       visualBellColor;
    uint32_t    visualBellDuration;

    bool        mapOnBell;
    bool        urgentOnBell;
    bool        audibleBell;
    bool        visualBell;

    bool        x11PseudoTransparency;
    bool        x11CompositedTransparency;
    double      x11TransparencyValue;

    //
    //
    //

    Config();

    void setColorScheme(const std::string & name);
};

#endif // COMMON__CONFIG__HXX
