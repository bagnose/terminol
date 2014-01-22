// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__DATA_TYPES__HXX
#define COMMON__DATA_TYPES__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/conv.hxx"

#include <algorithm>

//
// An RGB color.
//

struct Color {
    uint8_t r, g, b;

    Color() : r(0), g(0), b(0) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}
};

inline bool operator == (Color lhs, Color rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator != (Color lhs, Color rhs) {
    return !(lhs == rhs);
}

static_assert(sizeof(Color) == 3, "Color should be 3 bytes.");

std::ostream & operator << (std::ostream & ost, Color   color);
std::istream & operator >> (std::istream & ist, Color & color);

//
// Unified (hybrid) color.
//

struct UColor {
    enum class Type : uint8_t { STOCK, INDEXED, DIRECT };
    enum class Name : uint8_t {
        TEXT_FG, TEXT_BG, SELECT_FG, SELECT_BG, CURSOR_FILL, CURSOR_TEXT
    };

    Type type;              // 1 byte

    union {
        Name    name;       // 1 byte
        uint8_t index;      // 1 byte
        Color   values;     // 3 bytes

        uint8_t _init[3];
    };

    static UColor stock(Name name) {
        UColor ucolor(Type::STOCK);
        ucolor.name = name;
        return ucolor;
    }

    static UColor indexed(uint8_t index) {
        UColor ucolor(Type::INDEXED);
        ucolor.index = index;
        return ucolor;
    }

    static UColor direct(uint8_t r, uint8_t g, uint8_t b) {
        UColor ucolor(Type::DIRECT);
        ucolor.values.r = r;
        ucolor.values.g = g;
        ucolor.values.b = b;
        return ucolor;
    }

private:
    explicit UColor(Type type_) : type(type_), _init {0, 0, 0 } {}
};

static_assert(sizeof(UColor) == 4, "UColor should be 4 bytes.");

inline bool operator == (UColor lhs, UColor rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }
    else {
        switch (lhs.type) {
            case UColor::Type::STOCK:
                return lhs.name == rhs.name;
            case UColor::Type::INDEXED:
                return lhs.index == rhs.index;
            case UColor::Type::DIRECT:
                return lhs.values == rhs.values;
        }
    }

    FATAL("Unreachable");
}

inline bool operator != (UColor lhs, UColor rhs) { return !(lhs == rhs); }

//
// Style (of a Cell).
//

struct Style {
    AttrSet attrs;    // 1 byte
    UColor  fg;       // 4 bytes
    UColor  bg;       // 4 bytes

    Style() :
        attrs(),
        fg(UColor::stock(UColor::Name::TEXT_FG)),
        bg(UColor::stock(UColor::Name::TEXT_BG)) {}

    Style(AttrSet attrs_, UColor fg_, UColor bg_) :
        attrs(attrs_), fg(fg_), bg(bg_) {}
};

static_assert(sizeof(Style) == 9, "Style should be 9 bytes.");

inline bool operator == (const Style & lhs, const Style & rhs) {
    return
        lhs.attrs == rhs.attrs &&
        lhs.fg    == rhs.fg    &&
        lhs.bg    == rhs.bg;
}

inline bool operator != (const Style & lhs, const Style & rhs) {
    return !(lhs == rhs);
}

//
// Cell (an element in the Buffer array).
//

struct Cell {
    Style     style;        // 9 bytes
    utf8::Seq seq;          // 4 bytes

    static Cell blank(const Style & style = Style()) {
        return Cell(style, utf8::Seq(SPACE));
    }

    static Cell ascii(uint8_t a, const Style & style = Style()) {
        return Cell(style, utf8::Seq(a));
    }

    static Cell utf8(utf8::Seq seq, const Style & style = Style()) {
        return Cell(style, seq);
    }

private:
    Cell(const Style & style_, utf8::Seq seq_) :
        style(style_),
        seq(seq_) {}
};

static_assert(sizeof(Cell) == 13, "Incorrect size of Cell.");

inline bool operator == (const Cell & lhs, const Cell & rhs) {
    return
        lhs.seq   == rhs.seq &&
        lhs.style == rhs.style;
}

inline bool operator != (const Cell & lhs, const Cell & rhs) { return !(lhs == rhs); }

//
// A screen-relative position.
//

struct Pos {
    int16_t row;
    int16_t col;

    Pos() : row(0), col(0) {}
    Pos(int16_t row_, int16_t col_) : row(row_), col(col_) {}
};

inline bool operator == (Pos lhs, Pos rhs) {
    return lhs.row == rhs.row && lhs.col == rhs.col;
}

inline bool operator != (Pos lhs, Pos rhs) { return !(lhs == rhs); }

inline std::ostream & operator << (std::ostream & ost, Pos pos) {
    return ost << pos.row << 'x' << pos.col;
}

//
// A screen-relative position, with "handedness" (left/right).
//

struct HPos {
    Pos  pos;
    Hand hand;

    HPos() : pos(), hand(Hand::LEFT) {}
    HPos(int16_t row, int16_t col, Hand hand_) : pos(row, col), hand(hand_) {}

    static HPos invalid() {
        return HPos(-1, -1, Hand::LEFT);
    }
};

inline bool operator == (HPos lhs, HPos rhs) {
    return lhs.pos == rhs.pos && lhs.hand == rhs.hand;
}

inline bool operator != (HPos lhs, HPos rhs) { return !(lhs == rhs); }

inline std::ostream & operator << (std::ostream & ost, HPos hpos) {
    return ost << hpos.pos.row << 'x' << hpos.pos.col << '-' << hpos.hand;
}

//
// A region, defined by a beginning and an end (end is non-inclusive).
//

struct Region {
    Pos begin;
    Pos end;

    Region() : begin(), end() {}
    Region(Pos begin_, Pos end_) : begin(begin_), end(end_) {}

    void clear() { *this = Region(); }

    void accommodateCell(Pos pos) {
        accommodateRow(pos.row, pos.col, pos.col + 1);
    }

    void accommodateRow(int16_t row, int16_t colBegin, int16_t colEnd) {
        if (begin.col == end.col) {
            begin.col = colBegin;
            end.col   = colEnd;
        }
        else {
            begin.col = std::min(begin.col, colBegin);
            end.col   = std::max(end.col,   colEnd);
        }

        int16_t rowBegin = row;
        int16_t rowEnd   = row + 1;

        if (begin.row == end.row) {
            begin.row = rowBegin;
            end.row   = rowEnd;
        }
        else {
            begin.row = std::min(begin.row, rowBegin);
            end.row   = std::max(end.row,   rowEnd);
        }
    }
};

inline std::ostream & operator << (std::ostream & ost, const Region & region) {
    return ost << "begin: " << region.begin << ", end: " << region.end;
}

#endif // COMMON__DATA_TYPES__HXX
