// vi:noai:sw=4

#ifndef COMMON__CELL__HXX
#define COMMON__CELL__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"

#include <algorithm>

struct Color {
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    uint8_t r, g, b;
};

inline bool operator == (Color lhs, Color rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator != (Color lhs, Color rhs) {
    return !(lhs == rhs);
}

//
//
//

struct UColor {
    enum class Type : uint8_t { FOREGROUND, BACKGROUND, INDEXED, DIRECT };

    static UColor foreground() { return UColor(Type::FOREGROUND); }
    static UColor background() { return UColor(Type::BACKGROUND); }
    static UColor indexed(uint8_t index) { return UColor(index); }
    static UColor direct(uint8_t r, uint8_t g, uint8_t b) { return UColor(r, g, b); }

    Type type;

    union {
        uint8_t index;
        Color   values;
    };

private:
    explicit UColor(Type type_) : type(type_) {}
    explicit UColor(uint8_t index_) : type(Type::INDEXED), index(index_) {}
    UColor(uint8_t r, uint8_t g, uint8_t b) : type(Type::DIRECT), values(r, g, b) {}
};

inline bool operator == (UColor lhs, UColor rhs) {
    if (lhs.type != rhs.type) {
        return false;
    }
    else {
        switch (lhs.type) {
            case UColor::Type::FOREGROUND:
            case UColor::Type::BACKGROUND:
                return true;
            case UColor::Type::INDEXED:
                return lhs.index == rhs.index;
            case UColor::Type::DIRECT:
                return lhs.values == rhs.values;
        }
    }
}

inline bool operator != (UColor lhs, UColor rhs) { return !(lhs == rhs); }

//
//
//

struct Style {
    AttrSet attrs;    // 1 byte
    uint8_t padding;  // 1 byte - helpful ???
    UColor  fg;       // 4 bytes
    UColor  bg;       // 4 bytes

    Style() : attrs(), fg(UColor::foreground()), bg(UColor::background()) {}

    Style(AttrSet attrs_, UColor fg_, UColor bg_) :
        attrs(attrs_), fg(fg_), bg(bg_) {}
};

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
//
//

struct Cell {
    Style     style;        // 10 bytes
    utf8::Seq seq;          // 4  bytes

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
