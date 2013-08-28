// vi:noai:sw=4

#ifndef COMMON__DATA_TYPES__HXX
#define COMMON__DATA_TYPES__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/conv.hxx"

#include <algorithm>

struct Color {
    uint8_t r, g, b;

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
    enum class Type : uint8_t { STOCK, INDEXED, DIRECT };
    enum class Name : uint8_t {
        TEXT_FG, TEXT_BG, SELECT_FG, SELECT_BG, CURSOR_FILL, CURSOR_TEXT
    };

    Type type;

    union {
        Name    name;
        uint8_t index;
        Color   values;
    };

    static UColor stock(Name name) { return UColor(name); }
    static UColor indexed(uint8_t index) { return UColor(index); }
    static UColor direct(uint8_t r, uint8_t g, uint8_t b) { return UColor(r, g, b); }

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
//
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
//
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
