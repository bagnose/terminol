// vi:noai:sw=4

#include "terminol/common/utf8.hxx"
#include "terminol/common/support.hxx"

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
Length leadLength(uint8_t lead) throw (Error) {
    if ((lead & B7) == 0) {
        // 0xxxxxxx
        return Length::L1;
    }
    else if ((lead & (B7|B6|B5)) == (B7|B6)) {
        // 110xxxxx (10xxxxxx)
        return Length::L2;
    }
    else if ((lead & (B7|B6|B5|B4)) == (B7|B6|B5)) {
        // 1110xxxx (10xxxxxx) (10xxxxxx)
        return Length::L3;
    }
    else if ((lead & (B7|B6|B5|B4|B3)) == (B7|B6|B5|B4)) {
        // 11110xxx (10xxxxxx) (10xxxxxx) (10xxxxxx)
        return Length::L4;
    }
    else {
        FATAL("Cannot determine lead length: " << toBinary(lead));
        throw Error();
    }
}

CodePoint decode(const uint8_t * sequence) throw (Error) {
    CodePoint codePoint = 0;
    auto      lead      = sequence[0];
    auto      length    = leadLength(lead);

    // Handle lead.

    switch (length) {
        case Length::L1:
            // 0xxxxxxx
            codePoint = static_cast<CodePoint>(lead);
            break;
        case Length::L2:
            // 110xxxxx
            codePoint = static_cast<CodePoint>(lead & (B4|B3|B2|B1|B0));
            break;
        case Length::L3:
            // 1110xxxx
            codePoint = static_cast<CodePoint>(lead & (B3|B2|B1|B0));
            break;
        case Length::L4:
            // 11110xxx
            codePoint = static_cast<CodePoint>(lead & (B2|B1|B0));
            break;
    }

    // Handle continuations.

    for (size_t i = 1; i != length; ++i) {
        // 10xxxxxx
        auto cont = sequence[i];
        if ((cont & (B7|B6)) != B7) {
            FATAL("Illegal continuation: " << toBinary(cont));
            throw Error();
        }
        codePoint = (codePoint << 6) | (cont & (B5|B4|B3|B2|B1|B0));
    }

    // Check for overlong.

    switch (length) {
        case Length::L1:
            break;
        case Length::L2:
            if (codePoint < 0x80) {
                FATAL("Overlong 2, codePoint=" << codePoint);
                throw Error();
            }
            break;
        case Length::L3:
            if (codePoint < 0x800) {
                FATAL("Overlong 3, codePoint=" << codePoint);
                throw Error();
            }
            break;
        case Length::L4:
            if (codePoint < 0x10000) {
                FATAL("Overlong 4, codePoint=" << codePoint);
                throw Error();
            }
            break;
    }

    if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
        FATAL("Illegal codePoint: " << codePoint);
        throw Error();
    }

    return codePoint;
}

Length codePointLength(CodePoint codePoint) throw (Error) {
    if      (codePoint < 0x80) {
        return Length::L1;
    }
    else if (codePoint < 0x800) {
        return Length::L2;
    }
    else if (codePoint < 0x10000) {
        return Length::L3;
    }
    else if (codePoint < 0x110000) {
        return Length::L4;
    }
    else {
        FATAL("Illegal codePoint: " << codePoint);
        throw Error();
    }
}

Length encode(CodePoint codePoint, uint8_t * sequence) throw (Error) {
    Length length = codePointLength(codePoint);

    // Lead char.

    switch (length) {
        case Length::L1:
            // 0xxxxxxx
            sequence[0] = static_cast<uint8_t>(codePoint);
            break;
        case Length::L2:
            // 110xxxxx
            sequence[0] = static_cast<uint8_t>(codePoint >>  6) | (B7|B6);
            break;
        case Length::L3:
            // 1110xxxx
            sequence[0] = static_cast<uint8_t>(codePoint >> 12) | (B7|B6|B5);
            break;
        case Length::L4:
            // 11110xxx
            sequence[0] = static_cast<uint8_t>(codePoint >> 18) | (B7|B6|B5|B4);
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

//
//
//

std::ostream & operator << (std::ostream & ost, Seq seq) {
    Length l = leadLength(seq.bytes[0]);
    for (uint8_t i = 0; i != l; ++i) {
        ost << seq.bytes[i];
    }
    return ost;
}

//
//
//

Machine::State Machine::consume(uint8_t c) {
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

} // namespace utf8
