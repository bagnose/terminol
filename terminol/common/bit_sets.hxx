// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__BIT_SETS__HXX
#define COMMON__BIT_SETS__HXX

#include "terminol/common/enums.hxx"

#include <iosfwd>
#include <stdint.h>

template <typename T, typename I> class BitSet {
    I _bits;
    static I bit(T t) { return 1 << static_cast<int>(t); }

public:
    BitSet() : _bits(0) {}

    void clear()        { _bits  =  I(0);        }
    void set(T t)       { _bits |=  bit(t);      }
    void unset(T t)     { _bits &= ~bit(t);      }
    bool get(T t) const { return _bits & bit(t); }
    I    bits()   const { return _bits;          }

    void setTo(T t, bool to) {
        if (to) { set(t);   }
        else    { unset(t); }
    }

    friend inline bool operator == (BitSet lhs, BitSet rhs) {
        return lhs._bits == rhs._bits;
    }

    friend inline bool operator != (BitSet lhs, BitSet rhs) {
        return !(lhs == rhs);
    }
};

//
//
//

typedef BitSet<Modifier, uint8_t> ModifierSet;
std::ostream & operator << (std::ostream & ost, ModifierSet modifierSet);

typedef BitSet<Attr, uint8_t> AttrSet;
std::ostream & operator << (std::ostream & ost, AttrSet attrSet);

typedef BitSet<Mode, uint32_t> ModeSet;
std::ostream & operator << (std::ostream & ost, ModeSet modeSet);

#endif // COMMON__BIT_SETS__HXX
