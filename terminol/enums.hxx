// vi:noai:sw=4

#ifndef ENUMS__HXX
#define ENUMS__HXX

#include <iosfwd>

enum Control {
    CONTROL_BEL,
    CONTROL_HT,
    CONTROL_BS,
    CONTROL_CR,
    CONTROL_LF
};

std::ostream & operator << (std::ostream & ost, Control control);

//
//
//

enum ClearScreen {
    CLEAR_SCREEN_BELOW,
    CLEAR_SCREEN_ABOVE,
    CLEAR_SCREEN_ALL
};

std::ostream & operator << (std::ostream & ost, ClearScreen clear);

//
//
//

enum ClearLine {
    CLEAR_LINE_RIGHT,
    CLEAR_LINE_LEFT,
    CLEAR_LINE_ALL
};

std::ostream & operator << (std::ostream & ost, ClearLine clear);

//
//
//

enum Mode {
    MODE_WRAP,
    MODE_INSERT,
    MODE_APPKEYPAD,
    MODE_ALTSCREEN,
    MODE_CRLF,
    MODE_MOUSEBTN,
    MODE_MOUSEMOTION,
    // MOUSE = MOUSEBTN | MOUSEMOTION
    MODE_REVERSE,
    MODE_KBDLOCK,
    MODE_HIDE,
    MODE_ECHO,
    MODE_APPCURSOR,
    MODE_MOUSESGR,
    LAST_MODE = MODE_MOUSESGR
};

std::ostream & operator << (std::ostream & ost, Mode mode);

//
//
//

enum Attribute {
    ATTRIBUTE_BOLD,
    ATTRIBUTE_ITALIC,
    ATTRIBUTE_UNDERLINE,
    ATTRIBUTE_BLINK,
    ATTRIBUTE_REVERSE,
    LAST_ATTRIBUTE = ATTRIBUTE_REVERSE
};

std::ostream & operator << (std::ostream & ost, Attribute attribute);

#endif // ENUMS_HXX
