// vi:noai:sw=4

#ifndef CHAR__HXX
#define CHAR__HXX

#include "terminol/bit_sets.hxx"
#include "terminol/utf8.hxx"
#include "terminol/ascii.hxx"

#include <algorithm>

class Char {
    char         _bytes[utf8::LMAX];
    AttributeSet _attributes;
    uint8_t      _state;        // XXX what is this again??
    uint8_t      _fg;
    uint8_t      _bg;

    Char(const char   * bytes_,
         utf8::Length   length,
         AttributeSet   attributes_,
         uint8_t        state_,
         uint8_t        fg_,
         uint8_t        bg_) :
        _attributes(attributes_),
        _state(state_),
        _fg(fg_),
        _bg(bg_)
    {
        std::copy(bytes_, bytes_ + length, _bytes);
    }

    static const char NULL_CHAR = NUL;    // NUL/SPACE

public:
    static uint8_t  defaultFg()  { return 7; }
    static uint8_t  defaultBg()  { return 0; }

    static Char null() {
        return ascii(NULL_CHAR);
    }

    static Char ascii(char c) {
        return Char(&c, utf8::L1, AttributeSet(), 0, defaultFg(), defaultBg());
    }

    static Char utf8(const char   * s,
                     utf8::Length   length,
                     AttributeSet   attributes,
                     uint8_t        state,
                     uint8_t        fg,
                     uint8_t        bg)
    {
        return Char(s, length, attributes, state, fg, bg);
    }

    char         leadByte()   const { return _bytes[0]; }
    const char * bytes()      const { return _bytes; }
    AttributeSet attributes() const { return _attributes; }
    uint8_t      state()      const { return _state; }
    uint8_t      fg()         const { return _fg; }
    uint8_t      bg()         const { return _bg; }

    bool isNull() const { return _bytes[0] == NULL_CHAR; }
};

std::ostream & operator << (std::ostream & ost, const Char & ch);

#endif // CHAR__HXX
