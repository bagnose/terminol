// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/escape.hxx"

namespace {

const int32_t SGR_TABLE[] = {
    0,      // reset all

    1,      // bold
    2,      // faint
    3,      // italic
    4,      // underline
    5,      // blink slow
    6,      // blink rapid
    7,      // inverse / negative
    8,      // conceal

    22,     // clear weight
    23,     // clear slant
    24,     // clear underline
    25,     // clear blink
    27,     // clear inverse
    28,     // clear conceal

    30,     // fg black
    31,     // fg red
    32,     // fg green
    33,     // fg yellow
    34,     // fg blue
    35,     // fg magenta
    36,     // fg cyan
    37,     // fg white

    39,     // fg default

    40,     // bg black
    41,     // bg red
    42,     // bg green
    43,     // bg yellow
    44,     // bg blue
    45,     // bg magenta
    46,     // bg cyan
    47,     // bg white

    49,     // bg default

    90,     // fg bright black
    91,     // fg bright red
    92,     // fg bright green
    93,     // fg bright yellow
    94,     // fg bright blue
    95,     // fg bright magenta
    96,     // fg bright cyan
    97,     // fg bright white

    100,    // bg bright black
    101,    // bg bright red
    102,    // bg bright green
    103,    // bg bright yellow
    104,    // bg bright blue
    105,    // bg bright magenta
    106,    // bg bright cyan
    107     // bg bright white
};

} // namespace {anonymous}

std::ostream & operator << (std::ostream & ost, SGR sgr) {
    ost << '\033' << '[';
    ost << SGR_TABLE[static_cast<size_t>(sgr)];
    ost << 'm';
    return ost;
}

std::ostream & operator << (std::ostream & ost, MoveCursor moveCursor) {
    ost << '\033' << '['
        << moveCursor.row << ';'
        << moveCursor.col << 'H';
    return ost;
}
