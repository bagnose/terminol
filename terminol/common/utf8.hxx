// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__UTF8__HXX
#define COMMON__UTF8__HXX

#include "terminol/support/debug.hxx"

#include <cstdint>
#include <array>

namespace utf8 {

enum Length : uint8_t {
    L1 = 1,
    L2,
    L3,
    L4,
    LMAX = L4
};

using CodePoint = int32_t;

//
//
//

// Given a lead character, what is the length of the sequence?
Length    leadLength(uint8_t lead) noexcept;

// Decode a sequence into a code point. Assumes sequence is complete.
CodePoint decode(const uint8_t * sequence);

// Given a code point, what is the length of the sequence?
Length    codePointLength(CodePoint codePoint);

// Encode a code point into a sequence. Assumes sequence has sufficient allocation.
Length    encode(CodePoint codePoint, uint8_t * sequence);

bool      isLead(uint8_t byte) noexcept;

bool      isCont(uint8_t byte) noexcept;

//
//
//

struct Seq {
    constexpr Seq(uint8_t b0 = '\0', uint8_t b1 = '\0', uint8_t b2 = '\0', uint8_t b3 = '\0') noexcept :
        bytes{{b0, b1, b2, b3}} {}

    constexpr void clear() noexcept {
        bytes = {{'\0', '\0', '\0', '\0'}};
    }

    constexpr uint8_t lead() const noexcept { return bytes[0]; }

    std::array<uint8_t, Length::LMAX> bytes = {{'\0', '\0', '\0', '\0'}};
};

inline bool operator == (Seq lhs, Seq rhs) noexcept {
    return
        lhs.bytes[0] == rhs.bytes[0] &&
        lhs.bytes[1] == rhs.bytes[1] &&
        lhs.bytes[2] == rhs.bytes[2] &&
        lhs.bytes[3] == rhs.bytes[3];
}

inline bool operator != (Seq lhs, Seq rhs) noexcept {
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
    State   _state = State::START;
    uint8_t _index = 0;
    Seq     _seq;

public:
    Machine() noexcept = default;

    Length length() const noexcept {
        ASSERT(_state == State::ACCEPT, );
        return static_cast<Length>(_index);
    }

    Seq seq() const noexcept {
        ASSERT(_state == State::ACCEPT, );
        return _seq;
    }

    State consume(uint8_t c) noexcept;
};

} // namespace utf8

#endif // COMMON__UTF8__HXX
