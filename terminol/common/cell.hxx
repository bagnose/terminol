// vi:noai:sw=4

#ifndef COMMON__CELL__HXX
#define COMMON__CELL__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"

#include <algorithm>

// PACK_CELL  -> 8  bytes
// !PACK_CELL -> 10 bytes
#define PACK_CELL 1

class Cell {
    utf8::Seq _seq;      // 4 bytes
    AttrSet   _attrs;    // 1 byte
#if !PACK_CELL
    uint16_t  _fg;
    uint16_t  _bg;
#else
    uint8_t   _col[3];   // 3 bytes
#endif

    Cell(utf8::Seq seq_,
         AttrSet   attrs_,
         uint16_t  fg_,
         uint16_t  bg_) :
        _seq(seq_),
        _attrs(attrs_)
#if !PACK_CELL
        ,
        _fg(fg_),
        _bg(bg_) {}
#else
    {
        _col[0] = (fg_ >> 4) & 0xFF;
        _col[1] = ((fg_ << 4) & 0xF0) | ((bg_ >> 8) & 0x0F);
        _col[2] = (bg_ & 0xFF);
    }
#endif

    static const uint8_t BLANK = SPACE;

public:
    static uint16_t defaultFg()    { return 256 + 0; }
    static uint16_t defaultBg()    { return 256 + 1; }
    static AttrSet  defaultAttrs() { return AttrSet(); }

    static Cell blank() {
        return ascii(BLANK);
    }

    static Cell ascii(uint8_t c) {
        ASSERT((c & 0x80) == 0, "");
        return Cell(utf8::Seq(c), defaultAttrs(), defaultFg(), defaultBg());
    }

    static Cell utf8(utf8::Seq seq_,
                     AttrSet   attrs_,
                     uint16_t  fg_,
                     uint16_t  bg_) {
        return Cell(seq_, attrs_, fg_, bg_);
    }

    uint8_t         lead()  const { return _seq.bytes[0]; }
    const uint8_t * bytes() const { return &_seq.bytes[0]; }
    //utf8::Seq       seq()   const { return _seq; }
    AttrSet         attrs() const { return _attrs; }
#if !PACK_CELL
    uint16_t        fg()    const { return _fg; }
    uint16_t        bg()    const { return _bg; }
#else
    uint16_t fg() const {
        return (static_cast<int16_t>(_col[0]) << 4) | ((_col[1] >> 4) & 0x0F);
    }
    uint16_t bg() const {
        return ((static_cast<int16_t>(_col[1]) & 0x0F) << 8) | ((_col[2]));
    }
#endif

    friend inline bool operator == (Cell lhs, Cell rhs);
};

inline bool operator == (Cell lhs, Cell rhs) {
    return
        lhs._seq    == rhs._seq    &&
        lhs._attrs  == rhs._attrs  &&
#if !PACK_CELL
        lhs._fg     == rhs._fg     &&
        lhs._bg     == rhs._bg;
#else
        lhs._col[0] == rhs._col[0] &&
        lhs._col[1] == rhs._col[1] &&
        lhs._col[2] == rhs._col[2];
#endif
}

inline bool operator != (Cell lhs, Cell rhs) {
    return !(lhs == rhs);
}

std::ostream & operator << (std::ostream & ost, const Cell & cell);

#endif // COMMON__CELL__HXX
