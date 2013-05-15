// vi:noai:sw=4

#ifndef SUPPORT__ESCAPE__HXX
#define SUPPORT__ESCAPE__HXX

#include <iostream>

enum class Esc {
    RESET,

    BOLD,
    FAINT,
    ITALIC,
    UNDERLINE,
    BLINK,
    INVERSE,
    CONCEAL,

    CL_WEIGHT,      // remove bold/faint
    CL_SLANT,       // remove italic/fraktur
    CL_UNDERLINE,
    CL_BLINK,
    CL_INVERSE,
    CL_CONCEAL,
    CL_COLOR,

    FG_BLACK,
    FG_RED,
    FG_GREEN,
    FG_YELLOW,
    FG_BLUE,
    FG_MAGENTA,
    FG_CYAN,
    FG_WHITE,

    BG_BLACK,
    BG_RED,
    BG_GREEN,
    BG_YELLOW,
    BG_BLUE,
    BG_MAGENTA,
    BG_CYAN,
    BG_WHITE
};

std::ostream & operator << (std::ostream & ost, Esc e);

#endif // SUPPORT__ESCAPE__HXX
