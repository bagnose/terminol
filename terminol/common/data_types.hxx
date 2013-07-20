// vi:noai:sw=4

#ifndef COMMON__DATA_TYPES__HXX
#define COMMON__DATA_TYPES__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/conv.hxx"

#include <algorithm>

struct Color {
    Color() : r(0), g(0), b(0) {}
    Color(uint8_t r_, uint8_t g_, uint8_t b_) : r(r_), g(g_), b(b_) {}

    static Color fromString(const std::string & str) throw (ParseError) {
        if (str.size() != 7) {
            throw ParseError("Expected 7 characters");
        }

        if (str.front() != '#') {
            throw ParseError("Expected leading '#'");
        }

        return Color(hexToByte(str[1], str[2]),
                     hexToByte(str[3], str[4]),
                     hexToByte(str[5], str[6]));
    }

    uint8_t r, g, b;
};

inline bool operator == (Color lhs, Color rhs) {
    return lhs.r == rhs.r && lhs.g == rhs.g && lhs.b == rhs.b;
}

inline bool operator != (Color lhs, Color rhs) {
    return !(lhs == rhs);
}

//
// Unified (hybrid) color.
//

struct UColor {
    // TODO NORMAL_FG, CURSOR_FG
    enum class Type : uint8_t { STOCK, INDEXED, DIRECT };
    enum class Name : uint8_t {
        TEXT_FG, TEXT_BG, SELECT_FG, SELECT_BG, CURSOR_FILL, CURSOR_TEXT
    };

    static UColor stock(Name name) { return UColor(name); }
    static UColor indexed(uint8_t index) { return UColor(index); }
    static UColor direct(uint8_t r, uint8_t g, uint8_t b) { return UColor(r, g, b); }

    Type type;

    union {
        Name    name;
        uint8_t index;
        Color   values;
    };

private:
    explicit UColor(Name name_) : type(Type::STOCK), name(name_) {}
    explicit UColor(uint8_t index_) : type(Type::INDEXED), index(index_) {}
    UColor(uint8_t r, uint8_t g, uint8_t b) : type(Type::DIRECT), values(r, g, b) {}
};

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
//
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

    Pos begin;
    Pos end;

    void clear() { *this = Region(); }

    void accommodateCell(Pos pos) {
        accommodateRow(pos.row, pos.col, pos.col + 1);
    }

    void accommodateRow(uint16_t row, uint16_t colBegin, uint16_t colEnd) {
        if (begin.col == end.col) {
            begin.col = colBegin;
            end.col   = colEnd;
        }
        else {
            begin.col = std::min(begin.col, colBegin);
            end.col   = std::max(end.col,   colEnd);
        }

        uint16_t rowBegin = row;
        uint16_t rowEnd   = row + 1;

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

#endif // COMMON__DATA_TYPES__HXX
