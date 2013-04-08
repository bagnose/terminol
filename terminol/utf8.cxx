// vi:noai:sw=4

#include "terminol/utf8.hxx"
#include "terminol/common.hxx"

namespace utf8 {

const uint8_t B0 = 1 << 0;
const uint8_t B1 = 1 << 1;
const uint8_t B2 = 1 << 2;
const uint8_t B3 = 1 << 3;
const uint8_t B4 = 1 << 4;
const uint8_t B5 = 1 << 5;
const uint8_t B6 = 1 << 6;
const uint8_t B7 = 1 << 7;

// Just inspect the lead octect to determine the length.
Length leadLength(char lead) throw (Error) {
    if ((lead & B7) == 0) {
        // 0xxxxxxx
        return L1;
    }
    else if ((lead & (B7|B6|B5)) == (B7|B6)) {
        // 110xxxxx (10xxxxxx)
        return L2;
    }
    else if ((lead & (B7|B6|B5|B4)) == (B7|B6|B5)) {
        // 1110xxxx (10xxxxxx) (10xxxxxx)
        return L3;
    }
    else if ((lead & (B7|B6|B5|B4|B3)) == (B7|B6|B5|B4)) {
        // 11110xxx (10xxxxxx) (10xxxxxx) (10xxxxxx)
        return L4;
    }
    else {
        throw Error();
    }
}

CodePoint decode(const char * sequence) throw (Error) {
    CodePoint codePoint;
    auto      lead   = sequence[0];
    auto      length = leadLength(lead);

    // Handle lead.

    switch (length) {
        case L1:
            // 0xxxxxxx
            codePoint = static_cast<CodePoint>(lead);
            break;
        case L2:
            // 110xxxxx
            codePoint = static_cast<CodePoint>(lead & (B4|B3|B2|B1|B0));
            break;
        case L3:
            // 1110xxxx
            codePoint = static_cast<CodePoint>(lead & (B3|B2|B1|B0));
            break;
        case L4:
            // 11110xxx
            codePoint = static_cast<CodePoint>(lead & (B2|B1|B0));
            break;
    }

    // Handle continuations.

    for (size_t i = 1; i != length; ++i) {
        // 10xxxxxx
        auto cont = sequence[i];
        if ((cont & (B7|B6)) != B7) { throw Error(); }
        codePoint = (codePoint << 6) | (cont & (B5|B4|B3|B2|B1|B0));
    }

    // Check for overlong.

    switch (length) {
        case L1:
            break;
        case L2:
            if (codePoint < 0x80) { throw Error(); }
            break;
        case L3:
            if (codePoint < 0x800) { throw Error(); }
            break;
        case L4:
            if (codePoint < 0x10000) { throw Error(); }
            break;
    }

    if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
        throw Error();
    }

    return codePoint;
}

Length codePointLength(CodePoint codePoint) throw (Error) {
    if (codePoint < 0x80) {
        return L1;
    }
    else if (codePoint < 0x800) {
        return L2;
    }
    else if (codePoint < 0x10000) {
        return L3;
    }
    else if (codePoint <= 0x10FFFF) {
        return L4;
    }
    else {
        throw Error();
    }
}

Length encode(CodePoint codePoint, char * sequence) throw (Error) {
    Length length = codePointLength(codePoint);

    // Lead char.

    switch (length) {
        case L1:
            // 0xxxxxxx
            sequence[0] = static_cast<char>(codePoint);
            break;
        case L2:
            // 110xxxxx
            sequence[0] = static_cast<char>(codePoint >>  6) | (B7|B6);
            break;
        case L3:
            // 1110xxxx
            sequence[0] = static_cast<char>(codePoint >> 12) | (B7|B6|B5);
            break;
        case L4:
            // 11110xxx
            sequence[0] = static_cast<char>(codePoint >> 18) | (B7|B6|B5|B4);
            break;
    }

    // Continuation chars.

    for (size_t i = 1; i != length; ++i) {
        // 10xxxxxx
        sequence[i] =
            ((codePoint >> (6 * (length - 1 - i))) & (B5|B4|B3|B2|B1|B0)) | B7;
    }

    return length;
}

} // namespace utf8
