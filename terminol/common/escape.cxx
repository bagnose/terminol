// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/escape.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/conv.hxx"

#include <array>

namespace {

constexpr std::array<int32_t, static_cast<size_t>(CsiEsc::StockSGR::NUM_SGR)> SGR_TABLE = {{
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
}};

// [0x30..0x7F)
constexpr std::array<const char *, 0x7F - 0x30> SIMPLE_STR = {{
    nullptr,    // '0'              0x30
    nullptr,    // '1'
    nullptr,    // '2'
    nullptr,    // '3'
    nullptr,    // '4'
    nullptr,    // '5'
    nullptr,    // '6'
    "DECSC",    // '7'
    //
    "DECRC",    // '8'
    nullptr,    // '9'
    nullptr,    // ':'
    nullptr,    // ';'
    nullptr,    // '<'
    "DECKPAM",  // '='
    "DECKPNM",  // '>'
    nullptr,    // '?'
    //
    nullptr,    // '@'              0x40
    nullptr,    // 'A'
    nullptr,    // 'B'
    nullptr,    // 'C'
    "IND",      // 'D'
    "NEL",      // 'E'
    nullptr,    // 'F'
    nullptr,    // 'G'
    //
    "HTS",      // 'H'
    nullptr,    // 'I'
    nullptr,    // 'J'
    nullptr,    // 'K'
    nullptr,    // 'L'
    "RI",       // 'M'
    "SS2",      // 'N'
    "SS3",      // 'O'
    //
    nullptr,    // 'P'              0x50
    nullptr,    // 'Q'
    nullptr,    // 'R'
    nullptr,    // 'S'
    nullptr,    // 'T'
    nullptr,    // 'U'
    nullptr,    // 'V'
    nullptr,    // 'W'
    //
    nullptr,    // 'X'
    nullptr,    // 'Y'
    "DECID",    // 'Z'
    nullptr,    // '['
    nullptr,    // '\'
    nullptr,    // ']'
    nullptr,    // '^'
    nullptr,    // '_'
    //
    nullptr,    // '`'              0x60
    nullptr,    // 'a'
    nullptr,    // 'b'
    "RIS",      // 'c'
    nullptr,    // 'd'
    nullptr,    // 'e'
    nullptr,    // 'f'
    nullptr,    // 'g'
    //
    nullptr,    // 'h'
    nullptr,    // 'i'
    nullptr,    // 'j'
    nullptr,    // 'k'
    nullptr,    // 'l'
    nullptr,    // 'm'
    nullptr,    // 'n'
    nullptr,    // 'o'
    //
    nullptr,    // 'p'              0x70
    nullptr,    // 'q'
    nullptr,    // 'r'
    nullptr,    // 's'
    nullptr,    // 't'
    nullptr,    // 'u'
    nullptr,    // 'v'
    nullptr,    // 'w'
    //
    nullptr,    // 'x'
    nullptr,    // 'y'
    nullptr,    // 'z'
    nullptr,    // '{'
    nullptr,    // '|'
    nullptr,    // '}'
    nullptr     // '~'
}};

// [0x40..0x80)
const std::array<const char *, 0x80 - 0x40> CSI_STR = {{
    "ICH",      // '@'              0x40
    "CUU",      // 'A'
    "CUD",      // 'B'
    "CUF",      // 'C'
    "CUB",      // 'D'
    "CNL",      // 'E'
    "CPL",      // 'F'
    "CHA",      // 'G'
    //
    "CUP",      // 'H'
    "CHT",      // 'I'
    "ED",       // 'J'
    "EL",       // 'K'
    "IL",       // 'L'
    "DL",       // 'M'
    nullptr,    // 'N'      EF
    nullptr,    // 'O'      EA
    //
    "DCH",      // 'P'              0x50
    nullptr,    // 'Q'      SEE
    nullptr,    // 'R'      CPR
    "SU",       // 'S'
    "SD",       // 'T'
    nullptr,    // 'U'      NP
    nullptr,    // 'V'      PP
    "CTC",      // 'W'
    //
    "ECH",      // 'X'
    nullptr,    // 'Y'      CVT
    "CBT",      // 'Z'
    nullptr,    // '['      SRS
    nullptr,    // '\'      PTX
    nullptr,    // ']'      SDS
    nullptr,    // '^'      SIMD
    nullptr,    // '_'      5F
    //
    "HPA",      // '`'              0x60
    "HPR",      // 'a'
    "REP",      // 'b'
    "DA",       // 'c'
    "VPA",      // 'd'
    "VPR",      // 'e'
    "HVP",      // 'f'
    "TBC",      // 'g'
    //
    "SM",       // 'h'
    nullptr,    // 'i'      MC
    nullptr,    // 'j'      HPB
    nullptr,    // 'k'      VPB
    "RM",       // 'l'
    "SGR",      // 'm'
    "DSR",      // 'n'
    nullptr,    // 'o'      DAQ
    //
    nullptr,    // 'p'              0x70
    "SCA",      // 'q'
    "STBM",     // 'r'
    nullptr,    // 's'      save cursor?
    nullptr,    // 't'      window ops?
    nullptr,    // 'u'      restore cursor
    nullptr,    // 'v'
    nullptr,    // 'w'
    //
    nullptr,    // 'x'
    nullptr,    // 'y'
    nullptr,    // 'z'
    nullptr,    // '{'
    nullptr,    // '|'
    nullptr,    // '}'
    nullptr,    // '~'
    nullptr     // DEL
}};

} // namespace {anonymous}

std::string SimpleEsc::str() const {
    ASSERT(code >= 0x30 && code < 0x7F,
           << "Simple escape code out of range: " <<
           std::hex << static_cast<int>(code));

    std::ostringstream ost;

    // escape initiator
    ost << "^[";

    // intermediates
    for (auto i : inters) {
        ost << i;
    }

    // code
    auto str_ = SIMPLE_STR[code - 0x30];
    if (str_) {
        ost << '(' << str_ << ')';
    }
    else {
        ost << code;
    }

    return ost.str();
}

std::ostream & operator << (std::ostream & ost, const SimpleEsc & esc) {
    ost << '\033';

    for (auto i : esc.inters) {
        ost << i;
    }

    ost << esc.code;

    return ost;
}

//
//
//

CsiEsc CsiEsc::SGR(StockSGR sgr) {
    ASSERT(sgr != StockSGR::NUM_SGR, );
    CsiEsc esc;
    esc.args.push_back(SGR_TABLE[static_cast<size_t>(sgr)]);
    esc.mode = 'm';
    return esc;
}

std::string CsiEsc::str() const {
    ASSERT(mode >= 0x40 && mode < 0x80,
           << "CSI Escape mode out of range: " <<
           std::hex << static_cast<int>(mode));

    std::ostringstream ost;

    // CSI initiator
    ost << "^[[";

    // private
    if (priv != NUL) { ost << priv; }

    // arguments
    auto firstArg = true;
    for (auto a : args) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << a;
    }

    // intermediates
    for (auto i : inters) {
        ost << i;
    }

    // mode
    auto str_ = CSI_STR[mode - 0x40];
    if (str_) {
        ost << '(' << str_ << ')';
    }
    else {
        ost << mode;
    }

    return ost.str();
}

std::ostream & operator << (std::ostream & ost, const CsiEsc & esc) {
    ASSERT(esc.mode >= 0x40 && esc.mode < 0x80,
           << "CSI Escape mode out of range: " <<
           std::hex << static_cast<int>(esc.mode));

    // CSI initiator
    ost << "\033[";

    // private
    if (esc.priv != NUL) { ost << esc.priv; }

    // arguments
    auto firstArg = true;
    for (auto a : esc.args) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << a;
    }

    // intermediates
    for (auto i : esc.inters) {
        ost << i;
    }

    // mode
    ost << esc.mode;

    return ost;
}

//
//
//

std::string DcsEsc::str() const {
    std::ostringstream ost;

    // DCS initiator
    ost << "^[P";

    for (auto s : seq) {
        ost << s;
    }

    return ost.str();
}

std::ostream & operator << (std::ostream & ost, const DcsEsc & esc) {
    // DCS initiator
    ost << "\033P";

    for (auto s : esc.seq) {
        ost << s;
    }

    return ost;
}

//
//
//

std::string OscEsc::str() const {
    std::ostringstream ost;

    // OSC initiator
    ost << "^[]";

    // arguments
    auto firstArg = true;
    for (auto a : args) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << a;
    }

    return ost.str();
}

std::ostream & operator << (std::ostream & ost, const OscEsc & esc) {
    // OSC initiator
    ost << "\033]";

    // arguments
    auto firstArg = true;
    for (auto a : esc.args) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << a;
    }

    return ost;
}
