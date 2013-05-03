// vi:noai:sw=4

#ifndef COMMON__UTF8__HXX
#define COMMON__UTF8__HXX

#include "terminol/common/support.hxx"

#include <stddef.h>
#include <stdint.h>

namespace utf8 {

struct Error {};

enum Length : uint8_t {
    L1 = 1,
    L2,
    L3,
    L4,
    LMAX = L4
};

typedef int32_t CodePoint;

//
//
//

// Given a lead character, what is the length of the sequence?
Length    leadLength(uint8_t lead) throw (Error);

// Decode a sequence into a code point. Assumes sequence is complete.
CodePoint decode(const uint8_t * sequence) throw (Error);

// Given a code point, what is the length of the sequence?
Length    codePointLength(CodePoint codePoint) throw (Error);

// Encode a code point into a sequence. Assumes sequence has sufficient allocation.
Length    encode(CodePoint codePoint, uint8_t * sequence) throw (Error);

//
//
//

struct Seq {
    Seq(uint8_t b0 = '\0', uint8_t b1 = '\0', uint8_t b2 = '\0', uint8_t b3 = '\0') {
        bytes[0] = b0; bytes[1] = b1; bytes[2] = b2; bytes[3] = b3;
    }

    void clear() {
        bytes[0] = bytes[1] = bytes[2] = bytes[3] = '\0';
    }

    uint8_t bytes[LMAX];
};

inline bool operator == (Seq lhs, Seq rhs) {
    return
        *reinterpret_cast<const int32_t *>(&lhs.bytes[0]) ==
        *reinterpret_cast<const int32_t *>(&rhs.bytes[0]);
}

inline bool operator != (Seq lhs, Seq rhs) {
    return !(lhs == rhs);
}

const Seq REPLACEMENT(0xEF, 0xBF, 0xBD, 0x00);

std::ostream & operator << (std::ostream & ost, Seq seq);

//
//
//

class Machine {
public:
    enum class State {
        START,
        ACCEPT,
        REJECT,
        EXPECT3,
        EXPECT2,
        EXPECT1
    };

private:
    State   _state;
    uint8_t _index;
    Seq     _seq;

public:
    Machine() : _state(State::START), _index(0), _seq() {}

    Length length() const {
        ASSERT(_state == State::ACCEPT, "");
        return static_cast<Length>(_index);
    }

    Seq seq() const {
        ASSERT(_state == State::ACCEPT, "");
        return _seq;
    }

    State next(uint8_t c);
};

} // namespace utf8

#endif // COMMON__UTF8__HXX
