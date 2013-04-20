// vi:noai:sw=4

#ifndef ENUMS__HXX
#define ENUMS__HXX

#include <iosfwd>

#include <stdint.h>

enum class Control {
    BEL,
    BS,
    HT,
    LF,
    VT,
    FF,
    CR,
    SO,
    SI
};

std::ostream & operator << (std::ostream & ost, Control control);

//
//
//

enum class ClearScreen {
    BELOW,
    ABOVE,
    ALL
};

std::ostream & operator << (std::ostream & ost, ClearScreen clear);

//
//
//

enum class ClearLine {
    RIGHT,
    LEFT,
    ALL
};

std::ostream & operator << (std::ostream & ost, ClearLine clear);

//
//
//

// XXX What about SHOW_CURSOR, AUTOREPEAT,
// ALT_SENDS_ESC, DELETE_SENDS_DEL?
enum class Mode {
    WRAP,      // AUTOWRAP? YES!
    INSERT,
    APPKEYPAD,
    ALTSCREEN,
    CRLF,      // CR_ON_LF?
    MOUSEBTN,
    MOUSEMOTION,
    // MOUSE = MOUSEBTN | MOUSEMOTION
    REVERSE,   // INVERSE ?
    KBDLOCK,
    HIDE,
    ECHO,
    APPCURSOR,
    MOUSESGR,
    LAST = MOUSESGR
};

std::ostream & operator << (std::ostream & ost, Mode mode);

//
//
//

enum class Attribute {    // XXX what about CONCEALED?
    BOLD,
    ITALIC,
    UNDERLINE,
    BLINK,
    REVERSE,
    LAST = REVERSE
};

std::ostream & operator << (std::ostream & ost, Attribute attribute);

#endif // ENUMS_HXX
