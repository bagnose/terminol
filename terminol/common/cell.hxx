// vi:noai:sw=4

#ifndef COMMON__CELL__HXX
#define COMMON__CELL__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"

#include <algorithm>

struct Style {
    uint16_t fg;       // 2 bytes
    uint16_t bg;       // 2 bytes
    AttrSet  attrs;    // 1 byte
    uint8_t  padding;  // 1 byte - helpful ???

    static uint16_t defaultFg()    { return 256 + 0; }
    static uint16_t defaultBg()    { return 256 + 1; }
    static AttrSet  defaultAttrs() { return AttrSet(); }

    static Style normal() {
        return Style(defaultFg(), defaultBg(), defaultAttrs());
    }

    Style(uint16_t fg_, uint16_t bg_, AttrSet attrs_) :
        fg(fg_), bg(bg_), attrs(attrs_), padding(0) {}
};

inline bool operator == (Style lhs, Style rhs) {
    return
        lhs.fg    == rhs.fg &&
        lhs.bg    == rhs.bg &&
        lhs.attrs == rhs.attrs;
}

inline bool operator != (Style lhs, Style rhs) { return !(lhs == rhs); }

//
//
//

struct Cell {
    utf8::Seq seq;          // 4 bytes
    Style     style;        // 6 bytes

    static Cell blank() { return Cell(utf8::Seq(SPACE), Style::normal()); }
    static Cell ascii(uint8_t a, Style style) { return Cell(utf8::Seq(a), style); }
    static Cell utf8(utf8::Seq seq, Style style) { return Cell(seq, style); }

private:
    Cell(utf8::Seq seq_, Style style_) :
        seq(seq_),
        style(style_) {}
};

inline bool operator == (const Cell & lhs, const Cell & rhs) {
    return
        lhs.seq   == rhs.seq &&
        lhs.style == rhs.style;
}

inline bool operator != (const Cell & lhs, const Cell & rhs) {
    return !(lhs == rhs);
}

#endif // COMMON__CELL__HXX
