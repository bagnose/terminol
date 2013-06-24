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

enum class Modifier {
    SHIFT,
    ALT,
    CONTROL,
    SUPER,
    NUM_LOCK,
    SHIFT_LOCK,
    CAPS_LOCK,
    MODE_SWITCH,
    LAST = MODE_SWITCH
};

std::ostream & operator << (std::ostream & ost, Modifier);

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
    MOUSE_BUTTON,
    MOUSE_MOTION,
    MOUSE_SGR,
    BRACKETED_PASTE,
    META_8BIT,
    LAST = META_8BIT
};

std::ostream & operator << (std::ostream & ost, Mode mode);

//
//
//

enum class Attr {
    BOLD,
    FAINT,
    ITALIC,
    UNDERLINE,
    BLINK,
    INVERSE,
    CONCEAL,
    LAST = CONCEAL
};

std::ostream & operator << (std::ostream & ost, Attr attr);

//
//
//

enum class Resize {
    CLIP,
    PRESERVE,
    REFLOW
};

std::ostream & operator << (std::ostream & ost, Resize resize);
std::istream & operator >> (std::istream & ist, Resize & resize);

#endif // COMMON__ENUMS_HXX
