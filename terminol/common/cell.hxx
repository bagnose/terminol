// vi:noai:sw=4

#ifndef COMMON__CELL__HXX
#define COMMON__CELL__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"

#include <algorithm>

class Cell {
    utf8::Seq    _seq;
    AttributeSet _attrs;
    uint8_t      _fg;
    uint8_t      _bg;

    Cell(utf8::Seq      seq_,
         AttributeSet   attrs_,
         uint8_t        fg_,
         uint8_t        bg_) :
        _seq(seq_),
        _attrs(attrs_),
        _fg(fg_),
        _bg(bg_) {}

    static const uint8_t BLANK = SPACE;

public:
#if 1
    static uint8_t      defaultFg()    { return 12; }
    static uint8_t      defaultBg()    { return 8;  }
#else
    static uint8_t      defaultFg()    { return 7; }
    static uint8_t      defaultBg()    { return 0; }
#endif
    static AttributeSet defaultAttrs() { return AttributeSet(); }

    static Cell blank() {
        return ascii(BLANK);
    }

    static Cell ascii(uint8_t c) {
        ASSERT((c & 0x80) == 0, "");
        return Cell(utf8::Seq(c), defaultAttrs(), defaultFg(), defaultBg());
    }

    static Cell utf8(utf8::Seq    seq_,
                     AttributeSet attrs_,
                     uint8_t      fg_,
                     uint8_t      bg_) {
        return Cell(seq_, attrs_, fg_, bg_);
    }

    uint8_t         lead()  const { return _seq.bytes[0]; }
    const uint8_t * bytes() const { return &_seq.bytes[0]; }
    //utf8::Seq       seq()   const { return _seq; }
    AttributeSet    attrs() const { return _attrs; }
    uint8_t         fg()    const { return _fg; }
    uint8_t         bg()    const { return _bg; }

    friend inline bool operator == (Cell lhs, Cell rhs);
};

inline bool operator == (Cell lhs, Cell rhs) {
    return
        lhs._seq   == rhs._seq   &&
        lhs._attrs == rhs._attrs &&
        lhs._fg    == rhs._fg    &&
        lhs._bg    == rhs._bg;
}

inline bool operator != (Cell lhs, Cell rhs) {
    return !(lhs == rhs);
}

std::ostream & operator << (std::ostream & ost, const Cell & cell);

#endif // COMMON__CELL__HXX
