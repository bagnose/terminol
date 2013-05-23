// vi:noai:sw=4

#ifndef COMMON__ENUMS__HXX
#define COMMON__ENUMS__HXX

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

enum class Mode {
    AUTO_WRAP,
    AUTO_REPEAT,
    SHOW_CURSOR,
    ALT_SENDS_ESC,
    DELETE_SENDS_DEL,
    CR_ON_LF,
    INSERT,
    ECHO,
    KBDLOCK,
    APPKEYPAD,
    APPCURSOR,
    REVERSE,

    // Remainder are dubious...
    MOUSEBTN,
    MOUSEMOTION,
    // MOUSE = MOUSEBTN | MOUSEMOTION
    MOUSESGR,
    LAST = MOUSESGR
};

std::ostream & operator << (std::ostream & ost, Mode mode);

//
//
//

enum class Attr {
    BOLD,
    ITALIC,
    UNDERLINE,
    BLINK,
    INVERSE,
    CONCEAL,
    LAST = CONCEAL
};

std::ostream & operator << (std::ostream & ost, Attr attr);

#endif // COMMON__ENUMS_HXX
