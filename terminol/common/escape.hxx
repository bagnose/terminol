// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#ifndef SUPPORT__ESCAPE__HXX
#define SUPPORT__ESCAPE__HXX

#include <iostream>
#include <vector>
#include <cstdint>

//
// Simple Escape.
//

struct SimpleEsc {
    std::vector<uint8_t> inters;
    uint8_t              code = 0;

    // Convert to human readable string.
    std::string str() const;
};

std::ostream & operator << (std::ostream & ost, const SimpleEsc & esc);

//
// Control Sequence Initiator.
//

struct CsiEsc {
    CsiEsc() : priv('\0'), args(), inters(), mode('\0') {}

    // SGR - Select Graphic Recognition.
    enum class StockSGR {
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

    // SGR - Select Graphic Rendition.
    static CsiEsc SGR(StockSGR sgr);

    // CUP - Cursor Position.
    static CsiEsc CUP(int row, int col) {
        CsiEsc esc;
        esc.args.push_back(row);
        esc.args.push_back(col);
        esc.mode = 'H';
        return esc;
    }

    enum class Cols { _80, _132 };

    // DECCOLM - Select 80 or 132 columns per page.
    static CsiEsc DECCOLM(Cols cols) {
        CsiEsc esc;
        esc.priv = '?';
        esc.args.push_back(3);
        esc.mode = (cols == Cols::_80 ? 'l' : 'h');
        return esc;
    }

    // DECSTBM - Set top and bottom margins.
    static CsiEsc DECSTBM(int top, int bottom) {
        CsiEsc esc;
        esc.args.push_back(top);
        esc.args.push_back(bottom);
        esc.mode = 'r';
        return esc;
    }

    // Convert to human readable string.
    std::string str() const;

    uint8_t              priv;
    std::vector<int32_t> args;
    std::vector<uint8_t> inters;
    uint8_t              mode;
};

std::ostream & operator << (std::ostream & ost, const CsiEsc & esc);

//
// Device Control Sequence.
//

struct DcsEsc {
    std::vector<uint8_t> seq;

    // Convert to human readable string.
    std::string str() const;
};

std::ostream & operator << (std::ostream & ost, const DcsEsc & esc);

//
// Operating System Command.
//

struct OscEsc {
    std::vector<std::string> args;

    // Convert to human readable string.
    std::string str() const;
};

std::ostream & operator << (std::ostream & ost, const OscEsc & esc);

#endif // SUPPORT__ESCAPE__HXX
