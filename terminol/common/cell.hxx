// vi:noai:sw=4

#ifndef CELL__HXX
#define CELL__HXX

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/utf8.hxx"
#include "terminol/common/ascii.hxx"

#include <algorithm>

class Cell {
    // TODO use utf8::Seq
    char         _bytes[utf8::LMAX];        // UTF-8 sequence
    AttributeSet _attrs;
    uint8_t      _fg;
    uint8_t      _bg;

    Cell(const char   * bytes_,
         utf8::Length   length,
         AttributeSet   attrs_,
         uint8_t        fg_,
         uint8_t        bg_) :
        _attrs(attrs_),
        _fg(fg_),
        _bg(bg_)
    {
        std::copy(bytes_, bytes_ + length, _bytes);
#if DEBUG
        std::fill(_bytes + length, _bytes + utf8::LMAX, 0);
#endif
    }

    static const char BLANK = SPACE;

public:
    static uint8_t      defaultFg()    { return 7; }
    static uint8_t      defaultBg()    { return 0; }
    static AttributeSet defaultAttrs() { return AttributeSet(); }

    static Cell blank() {
        return ascii(BLANK);
    }

    static Cell ascii(char c) {
        return Cell(&c, utf8::L1, defaultAttrs(), defaultFg(), defaultBg());
    }

    static Cell utf8(const char   * s,
                     utf8::Length   length,
                     AttributeSet   attributes,
                     uint8_t        fg,
                     uint8_t        bg) {
        return Cell(s, length, attributes, fg, bg);
    }

    char         lead()    const { return _bytes[0]; }
    const char * bytes()   const { return _bytes; }
    AttributeSet attrs()   const { return _attrs; }
    uint8_t      fg()      const { return _fg; }
    uint8_t      bg()      const { return _bg; }

    bool         isBlank() const { return _bytes[0] == BLANK; }
};

std::ostream & operator << (std::ostream & ost, const Cell & cell);

#endif // CELL__HXX
