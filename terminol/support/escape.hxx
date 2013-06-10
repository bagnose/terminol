// vi:noai:sw=4

#ifndef SUPPORT__ESCAPE__HXX
#define SUPPORT__ESCAPE__HXX

#include <iostream>

enum class SGR {
    RESET_ALL,          // (normal)

    BOLD,               // or increased intensity
    FAINT,              // (decreased intensity)
    ITALIC,
    UNDERLINE,
    BLINK_SLOW,
    BLINK_RAPID,
    INVERSE,            // (negative)
    CONCEAL,

    RESET_WEIGHT,       // remove bold/faint
    RESET_SLANT,        // remove italic
    RESET_UNDERLINE,
    RESET_BLINK,
    RESET_INVERSE,      // (positive)
    RESET_CONCEAL,      // (reveal)

    FG_BLACK,
    FG_RED,
    FG_GREEN,
    FG_YELLOW,
    FG_BLUE,
    FG_MAGENTA,
    FG_CYAN,
    FG_WHITE,

    FG_DEFAULT,

    BG_BLACK,
    BG_RED,
    BG_GREEN,
    BG_YELLOW,
    BG_BLUE,
    BG_MAGENTA,
    BG_CYAN,
    BG_WHITE,

    BG_DEFAULT,

    FG_BRIGHT_BLACK,
    FG_BRIGHT_RED,
    FG_BRIGHT_GREEN,
    FG_BRIGHT_YELLOW,
    FG_BRIGHT_BLUE,
    FG_BRIGHT_MAGENTA,
    FG_BRIGHT_CYAN,
    FG_BRIGHT_WHITE,

    BG_BRIGHT_BLACK,
    BG_BRIGHT_RED,
    BG_BRIGHT_GREEN,
    BG_BRIGHT_YELLOW,
    BG_BRIGHT_BLUE,
    BG_BRIGHT_MAGENTA,
    BG_BRIGHT_CYAN,
    BG_BRIGHT_WHITE,
};

std::ostream & operator << (std::ostream & ost, SGR sgr);

#endif // SUPPORT__ESCAPE__HXX
