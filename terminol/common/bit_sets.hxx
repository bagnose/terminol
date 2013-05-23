// vi:noai:sw=4

#ifndef COMMON__BIT_SETS__HXX
#define COMMON__BIT_SETS__HXX

#include "terminol/common/enums.hxx"

#include <iosfwd>
#include <stdint.h>

class AttrSet {
    uint8_t _bits;
    static uint8_t bit(Attr attr) { return 1 << static_cast<int>(attr); }

public:
    AttrSet() : _bits(0) {}

    void clear()              { _bits  =  0;              }
    void set(Attr attr)       { _bits |=  bit(attr);      }
    void unset(Attr attr)     { _bits &= ~bit(attr);      }
    bool get(Attr attr) const { return _bits & bit(attr); }

    void setTo(Attr attr, bool to) {
        if (to) { set(attr);   }
        else    { unset(attr); }
    }

    friend inline bool operator == (AttrSet lhs, AttrSet rhs);
};

std::ostream & operator << (std::ostream & ost, AttrSet attributeSet);

inline bool operator == (AttrSet lhs, AttrSet rhs) {
    return lhs._bits == rhs._bits;
}

inline bool operator != (AttrSet lhs, AttrSet rhs) {
    return !(lhs == rhs);
}

//
//
//

class ModeSet {
    uint16_t _bits;
    static uint16_t bit(Mode mode) { return 1 << static_cast<int>(mode); }

public:
    ModeSet() : _bits(0) {}

    void clear()              { _bits  =  0;              }
    void set(Mode mode)       { _bits |=  bit(mode);      }
    void unset(Mode mode)     { _bits &= ~bit(mode);      }
    bool get(Mode mode) const { return _bits & bit(mode); }

    void setTo(Mode mode, bool to) {
        if (to) { set(mode);   }
        else    { unset(mode); }
    }
};

std::ostream & operator << (std::ostream & ost, ModeSet modeSet);

#endif // COMMON__BIT_SETS__HXX
