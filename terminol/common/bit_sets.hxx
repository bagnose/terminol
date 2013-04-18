// vi:noai:sw=4

#ifndef BIT_SETS__HXX
#define BIT_SETS__HXX

#include "terminol/common/enums.hxx"

#include <iosfwd>
#include <stdint.h>

class AttributeSet {
    uint8_t _bits;
    static uint8_t bit(Attribute attribute) { return 1 << attribute; }

public:
    AttributeSet() : _bits(0) {}

    void clear()                        { _bits  =  0;                   }
    void set(Attribute attribute)       { _bits |=  bit(attribute);      }
    void unset(Attribute attribute)     { _bits &= ~bit(attribute);      }
    bool get(Attribute attribute) const { return _bits & bit(attribute); }

    void setTo(Attribute attribute, bool to) {
        if (to) { set(attribute);   }
        else    { unset(attribute); }
    }

    friend inline bool operator == (AttributeSet lhs, AttributeSet rhs);
};

std::ostream & operator << (std::ostream & ost, AttributeSet attributeSet);

inline bool operator == (AttributeSet lhs, AttributeSet rhs) {
    return lhs._bits == rhs._bits;
}

inline bool operator != (AttributeSet lhs, AttributeSet rhs) {
    return !(lhs == rhs);
}

//
//
//

class ModeSet {
    uint16_t _bits;
    static uint16_t bit(Mode mode) { return 1 << mode; }

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

#endif // BIT_SETS__HXX
