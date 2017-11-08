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

    std::string fontName            = "Monospace";
    int         fontSize            = 12;
    std::string termName            = "xterm-256color";
    bool        scrollWithHistory   = false;
    bool        scrollOnTtyOutput   = false;
    bool        scrollOnTtyKeyPress = true;
    bool        scrollOnResize      = false;
    bool        scrollOnPaste       = true;
    std::string title               = "terminol";
    std::string icon                = "terminol";
    std::string chdir;
    size_t      scrollBackHistory   = 1'000'000;
    bool        unlimitedScrollBack = true;
    int         framesPerSecond     = 50;
    bool        traditionalWrapping = false;
    // Debugging support:
    bool traceTty = false;
    bool syncTty  = false;
    //
    int16_t  initialX    = -1;
    int16_t  initialY    = -1;
    uint16_t initialRows = 24;
    uint16_t initialCols = 80;

    Color                 normalFgColor;
    Color                 normalBgColor;
    std::array<Color, 16> systemColors;

    // TODO use std::optional for optional Colors
    bool  customSelectBgColor = false;
    Color selectBgColor;
    bool  customSelectFgColor = false;
    Color selectFgColor;

    bool  customCursorFillColor = false;
    Color cursorFillColor;
    bool  customCursorTextColor = false;
    Color cursorTextColor;

    Color scrollbarFgColor;
    Color scrollbarBgColor;
    bool  scrollbarVisible = true;
    int   scrollbarWidth   = 6;

    Color    borderColor;
    int      borderThickness    = 1;
    uint32_t doubleClickTimeout = 400;

    std::string socketPath;
    bool        serverFork = true;

    Bindings bindings;

    std::string cutChars = "-A-Za-z0-9./?%&#_=+@~";

    bool autoHideCursor = true;

    bool mapOnBell    = false;
    bool urgentOnBell = false;
    bool audibleBell  = false;
    bool visualBell   = false;

    int      audibleBellVolume = 100;
    Color    visualBellColor;
    uint32_t visualBellDuration = 25;

    bool   x11PseudoTransparency     = false;
    bool   x11CompositedTransparency = false;
    double x11TransparencyValue      = 0.1;

    //
    //
    //

    Config();

    void setColorScheme(const std::string & name);
};

#endif // COMMON__CONFIG__HXX
