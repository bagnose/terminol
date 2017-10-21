// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/utf8.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

//const uint8_t B0 = 1 << 0;
const uint8_t B1 = 1 << 1;
const uint8_t B2 = 1 << 2;
//const uint8_t B3 = 1 << 3;
const uint8_t B4 = 1 << 4;
const uint8_t B5 = 1 << 5;
const uint8_t B6 = 1 << 6;
const uint8_t B7 = 1 << 7;

using namespace utf8;

namespace {

std::ostream & showCodePointBits(std::ostream & ost, CodePoint cp) {
    size_t s     = sizeof cp;
    bool  active = false;
    bool  space  = false;

    for (size_t i = 0; i != s; ++i) {
        if (space) { ost << " "; }
        uint8_t byte = static_cast<uint8_t>(cp >> (8 * (3 - i)));
        active = active || (byte != 0) || i == s - 1;
        if (active) {
            ost << toBinaryString(byte);
            //showBits(ost, byte);
        }
        space = active;
    }

    return ost;
}

std::ostream & showSeqBytes(std::ostream & ost, const uint8_t * seq) {
    Length l = leadLength(seq[0]);
    bool space = false;

    for (size_t i = 0; i != l; ++i) {
        if (space) { ost << " "; }
        uint8_t byte = seq[i];
        ost << nibbleToHex(byte >> 4) << nibbleToHex(byte & 0x0F);
        space = true;
    }

    return ost;
}

std::ostream & showSeqBits(std::ostream & ost, const uint8_t * seq) {
    Length l = leadLength(seq[0]);
    bool space = false;

    for (size_t i = 0; i != l; ++i) {
        if (space) { ost << " "; }
        else       { space = true; }
        uint8_t byte = seq[i];
        ost << toBinaryString(byte);
    }

    return ost;
}

void forwardReverse(CodePoint cp) {
    std::cout << "Original code point: ";
    std::cout << toHexString(cp) << std::endl;
    std::cout << toBinaryString(cp) << std::endl;

    uint8_t seq[LMAX];
    encode(cp, seq);

    std::cout << "Converted to sequence: ";
    showSeqBytes(std::cout, seq) << std::endl;
    showSeqBits(std::cout, seq) << std::endl;
    std::cout
        << "Sequence is: '" << std::string(seq, seq + leadLength(seq[0])) << "'"
        << std::endl;

    CodePoint cp2 = decode(seq);

    std::cout << "Back to code point: ";
    std::cout << toHexString(cp2) << std::endl;
    std::cout << toBinaryString(cp2) << std::endl << std::endl;

    ENFORCE(cp == cp2, cp << " = " << cp2);
}

} // namespace {anonymous}

int main() {
    ENFORCE(leadLength(B1) == Length::L1, "");
    ENFORCE(leadLength(B1 | B2) == Length::L1, "");
    ENFORCE(leadLength(static_cast<uint8_t>(~B7)) == Length::L1, "");
    ENFORCE(leadLength('a') == Length::L1, "");
    ENFORCE(leadLength('z') == Length::L1, "");
    ENFORCE(leadLength('\x7F') == Length::L1, "");

    ENFORCE(leadLength(B7 | B6) == Length::L2, "");
    ENFORCE(leadLength(B7 | B6 | B5) == Length::L3, "");
    ENFORCE(leadLength(B7 | B6 | B5 | B4) == Length::L4, "");

    // 1 byte sequence
    forwardReverse(0x00);
    forwardReverse(0x50);
    forwardReverse(0x7F);

    // 2 byte sequence
    forwardReverse(0x80);
    forwardReverse(0xFF);
    forwardReverse(0x0250);

    // 3 byte sequence
    forwardReverse(0x8250);

    // 4 byte sequence
    forwardReverse(0x038250);
    forwardReverse(0x10FFFF);

    return 0;
}
