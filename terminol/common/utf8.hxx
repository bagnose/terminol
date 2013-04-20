// vi:noai:sw=4

#ifndef UTF8__HXX
#define UTF8__HXX

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
Length    leadLength(char lead) throw (Error);

// Decode a sequence into a code point. Assumes sequence is complete.
CodePoint decode(const char * sequence) throw (Error);

// Given a code point, what is the length of the sequence?
Length    codePointLength(CodePoint codePoint) throw (Error);

// Encode a code point into a sequence. Assumes sequence has sufficient allocation.
Length    encode(CodePoint codePoint, char * sequence) throw (Error);

//
//
//

struct Seq {
    Seq(char b0 = '\0', char b1 = '\0', char b2 = '\0', char b3 = '\0') {
        bytes[0] = b0; bytes[1] = b1; bytes[2] = b2; bytes[3] = b3;
    }

    void clear() {
        bytes[0] = bytes[1] = bytes[2] = bytes[3] = '\0';
    }

    char bytes[LMAX];
};

const Seq REPLACEMENT(0xEF, 0xBF, 0xBD, 0x00);

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

    State next(char c) {
        switch (_state) {
            case State::START:
            case State::ACCEPT:
            case State::REJECT:
                _index = 0;
                _seq.clear();
                if (c == 0xC0 || c == 0xC1) {
                    // Overlong encoding, reject.
                    _state = State::REJECT;
                }
                else if ((c & 0x80) == 0) {
                    // Single byte, accept.
                    _seq.bytes[_index++] = c;
                    _state = State::ACCEPT;
                }
                else if ((c & 0xC0) == 0x80) {
                    // Parser out of sync, ignore byte.
                    _state = State::START;
                }
                else if ((c & 0xE0) == 0xC0) {
                    // Start of two byte sequence.
                    _seq.bytes[_index++] = c;
                    _state = State::EXPECT1;
                }
                else if ((c & 0xF0) == 0xE0) {
                    // Start of three byte sequence.
                    _seq.bytes[_index++] = c;
                    _state = State::EXPECT2;
                }
                else if ((c & 0xF8) == 0xF0) {
                    // Start of four byte sequence.
                    _seq.bytes[_index++] = c;
                    _state = State::EXPECT3;
                }
                else {
                    // Overlong encoding, reject.
                    _state = State::REJECT;
                }
                break;
            case State::EXPECT3:
                _seq.bytes[_index++] = c;
                if ((c & 0xC0) == 0x80) {
                    // All good, continue.
                    _state = State::EXPECT2;
                }
                else {
                    // Missing extra byte, reject.
                    _state = State::REJECT;
                }
                break;
            case State::EXPECT2:
                _seq.bytes[_index++] = c;
                if ((c & 0xC0) == 0x80) {
                    // All good, continue.
                    _state = State::EXPECT1;
                }
                else {
                    // Missing extra byte, reject.
                    _state = State::REJECT;
                }
                break;
            case State::EXPECT1:
                _seq.bytes[_index++] = c;
                if ((c & 0xC0) == 0x80) {
                    // All good, accept.
                    _state = State::ACCEPT;
                }
                else {
                    // Missing extra byte, reject.
                    _state = State::REJECT;
                }
                break;
        }

        return _state;
    }
};

} // namespace utf8

#endif // UTF8__HXX
