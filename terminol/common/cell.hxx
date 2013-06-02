// vi:noai:sw=4

#ifndef COMMON__CELL__HXX
#define COMMON__CELL__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"

#include <algorithm>

struct Triplet {
    Triplet(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    uint8_t r, g, b;
};

inline bool operator == (Triplet lhs, Triplet rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator != (Triplet lhs, Triplet rhs) { return !(lhs == rhs); }

//
//
//

struct Colr {
    explicit Colr(uint16_t index_) : type(Type::INDEXED), index(index_) {}
    Colr(uint8_t r, uint8_t g, uint8_t b) : type(Type::DIRECT), triplet(r, g, b) {}

    enum class Type : uint8_t { INDEXED, DIRECT };

    Type type;

    union {
        uint16_t index;
        Triplet  triplet;
    };
};

inline bool operator == (Colr lhs, Colr rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }
    else {
        if (lhs.type == Colr::Type::INDEXED) {
            return lhs.index == rhs.index;
        }
        else {
            return lhs.triplet == rhs.triplet;
        }
    }
}

inline bool operator != (Colr lhs, Colr rhs) { return !(lhs == rhs); }

//
//
//

struct Style {
    AttrSet attrs;    // 1 byte
    uint8_t padding;  // 1 byte - helpful ???
    Colr    fg;       // 4 bytes
    Colr    bg;       // 4 bytes

    static AttrSet defaultAttrs() { return AttrSet();     }
    static Colr    defaultFg()    { return Colr(256 + 0); }
    static Colr    defaultBg()    { return Colr(256 + 1); }

    static Style normal() {
        return Style(defaultAttrs(), defaultFg(), defaultBg());
    }

    Style(AttrSet attrs_, Colr fg_, Colr bg_) :
        attrs(attrs_), padding(0), fg(fg_), bg(bg_) {}
};

inline bool operator == (Style lhs, Style rhs) {
    return
        lhs.attrs == rhs.attrs &&
        lhs.fg    == rhs.fg    &&
        lhs.bg    == rhs.bg;
}

inline bool operator != (Style lhs, Style rhs) { return !(lhs == rhs); }

//
//
//

struct Cell {
    Style     style;        // 10 bytes
    utf8::Seq seq;          // 4  bytes

    static Cell blank() { return Cell(Style::normal(), utf8::Seq(SPACE)); }
    static Cell ascii(Style style, uint8_t a) { return Cell(style, utf8::Seq(a)); }
    static Cell utf8(Style style, utf8::Seq seq) { return Cell(style, seq); }

private:
    Cell(Style style_, utf8::Seq seq_) :
        style(style_),
        seq(seq_) {}
};

inline bool operator == (const Cell & lhs, const Cell & rhs) {
    return
        lhs.seq   == rhs.seq &&
        lhs.style == rhs.style;
}

inline bool operator != (const Cell & lhs, const Cell & rhs) { return !(lhs == rhs); }

//
//
//

struct Pos {
    Pos() : row(0), col(0) {}
    Pos(uint16_t row_, uint16_t col_) : row(row_), col(col_) {}

    uint16_t row;
    uint16_t col;

    static Pos invalid() {
        return Pos(std::numeric_limits<uint16_t>::max(),
                   std::numeric_limits<uint16_t>::max());
    }

    Pos atRow(uint16_t row_) { return Pos(row_, col); }
    Pos atCol(uint16_t col_) { return Pos(row, col_); }

    Pos left(uint16_t n = 1) const {
        if (n < col) { return Pos(row, col - n); }
        else         { return Pos(row, 0); }
    }

    Pos right(uint16_t n = 1) const { return Pos(row, col + n); }

    Pos up(uint16_t n = 1) const {
        if (n < row) { return Pos(row - n, col); }
        else         { return Pos(0, col); }
    }

    Pos down(uint16_t n = 1) const { return Pos(row + n, col); }
};

inline bool operator == (Pos lhs, Pos rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

inline bool operator != (Pos lhs, Pos rhs) { return !(lhs == rhs); }

inline std::ostream & operator << (std::ostream & ost, Pos pos) {
    return ost << pos.row << 'x' << pos.col;
}

//
//
//

struct Region {
    Region() : begin(), end() {}
    Region(Pos begin_, Pos end_) : begin(begin_), end(end_) {}

    void clear() { *this = Region(); }
    Pos begin;
    Pos end;
};

#endif // COMMON__CELL__HXX
