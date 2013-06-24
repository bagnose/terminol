// vi:noai:sw=4

#ifndef COMMON__CONFIG__HXX
#define COMMON__CONFIG__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/common/data_types.hxx"

struct Config {
    // TODO
    // pointerfgColor
    // pointerBgColor
    // scrollbarStrategy: show/hide/auto

    // titleUpdateStrategy: replace, append, prepend, ignore

    // allow blink

    // flow control?

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
    size_t      reflowHistory;
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

    Color       fgColor;
    Color       bgColor;
    Color       systemColors[16];

    bool        customCursorFillColor;
    Color       cursorFillColor;
    bool        customCursorTextColor;
    Color       cursorTextColor;

    Color       scrollbarFgColor;
    Color       scrollbarBgColor;
    int         scrollbarWidth;

    Color       borderColor;
    int         borderThickness;
    uint32_t    doubleClickTimeout;

    std::string socketPath;
    bool        serverFork;

    //
    //
    //

    Config();

    void setColorScheme(const std::string & name);
};

#endif // COMMON__CONFIG__HXX
