// vi:noai:sw=4

#ifndef COMMON__BIT_SETS__HXX
#define COMMON__BIT_SETS__HXX

#include "terminol/common/enums.hxx"

#include <iosfwd>
#include <stdint.h>

template <typename T, typename I> class GenericSet {
    I _bits;
    static I bit(T t) { return 1 << static_cast<int>(t); }

public:
    GenericSet() : _bits(0) {}

    void clear()        { _bits  =  I(0);        }
    void set(T t)       { _bits |=  bit(t);      }
    void unset(T t)     { _bits &= ~bit(t);      }
    bool get(T t) const { return _bits & bit(t); }

    void setTo(T t, bool to) {
        if (to) { set(t);   }
        else    { unset(t); }
    }

    friend inline bool operator == (GenericSet lhs, GenericSet rhs) {
        return lhs._bits == rhs._bits;
    }

    friend inline bool operator != (GenericSet lhs, GenericSet rhs) {
        return !(lhs == rhs);
    }
};

//
//
//


typedef GenericSet<Attr, uint8_t> AttrSet;
std::ostream & operator << (std::ostream & ost, AttrSet attrSet);

typedef GenericSet<Mode, uint16_t> ModeSet;
std::ostream & operator << (std::ostream & ost, ModeSet modeSet);

#endif // COMMON__BIT_SETS__HXX
