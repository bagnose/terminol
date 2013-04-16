// vi:noai:sw=4

#ifndef ENUMS__HXX
#define ENUMS__HXX

#include <iosfwd>

enum Control {
    CONTROL_BEL,
    CONTROL_BS,
    CONTROL_HT,
    CONTROL_LF,
    CONTROL_VT,
    CONTROL_FF,
    CONTROL_CR,
    CONTROL_SO,
    CONTROL_SI
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

// XXX What about MODE_SHOW_CURSOR, MODE_AUTOREPEAT,
// MODE_ALT_SENDS_ESC, DELETE_SENDS_DEL?
enum Mode {
    MODE_WRAP,      // AUTOWRAP? YES!
    MODE_INSERT,
    MODE_APPKEYPAD,
    MODE_ALTSCREEN,
    MODE_CRLF,      // CR_ON_LF?
    MODE_MOUSEBTN,
    MODE_MOUSEMOTION,
    // MOUSE = MOUSEBTN | MOUSEMOTION
    MODE_REVERSE,   // INVERSE ?
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

enum Attribute {    // XXX what about ATTRIBUTE_CONCEALED?
    ATTRIBUTE_BOLD,
    ATTRIBUTE_ITALIC,
    ATTRIBUTE_UNDERLINE,
    ATTRIBUTE_BLINK,
    ATTRIBUTE_REVERSE,
    LAST_ATTRIBUTE = ATTRIBUTE_REVERSE
};

std::ostream & operator << (std::ostream & ost, Attribute attribute);

#endif // ENUMS_HXX
