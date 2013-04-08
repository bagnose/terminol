// vi:noai:sw=4

#ifndef UTF8__HXX
#define UTF8__HXX

#include <stddef.h>
#include <stdint.h>

namespace utf8 {

struct Error {};

enum Length {
    L1 = 1,
    L2,
    L3,
    L4,
    LMAX = L4
};

typedef int32_t CodePoint;

// Given a lead character, what is the length of the sequence?
Length    leadLength(char lead) throw (Error);

// Decode a sequence into a code point. Assumes sequence is valid and domplete.
CodePoint decode(const char * sequence) throw (Error);

// Given a code point, what is the length of the sequence?
Length    codePointLength(CodePoint codePoint) throw (Error);

// Encode a code point into a sequence. Assumes sequence has sufficient allocation.
Length    encode(CodePoint codePoint, char * sequence) throw (Error);

} // namespace utf8

const int UTF_SIZ = 4;

#endif // UTF8__HXX
