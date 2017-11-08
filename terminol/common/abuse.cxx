// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/ascii.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

#include <cstdlib>

// FIXME model this off the state machine

namespace {

    int randomInt(int min, int max /* exclusive */) { return min + (random() % (max - min)); }

    uint8_t randomChar(int min, int max /* exclusive */) {
        return static_cast<uint8_t>(randomInt(min, max));
    }

    bool possibility(int percent) { return randomInt(0, 100) < percent; }

    void writeRandomControl(std::ostream & ost) {
        uint8_t c;

        do { c = randomChar(0x0, 0x20); } while (c == 0x18 || c == 0x1A || c == 0x1B);

        ost << c;
    }

    void writeRandomEsc(std::ostream & ost) {
        ost << ESC;

        while (possibility(20)) {
            // Intermediates
            ost << randomChar(0x20, 0x30);
        }

        uint8_t c;

        do {
            c = randomChar(0x30, 0x7F);
        } while (c == 0x50 || c == 0x58 || c == 0x5B || c == 0x5D || c == 0x5E || c == 0x5F);

        ost << c;
    }

    void writeRandomCsi(std::ostream & ost) {
        ost << ESC << '[';

        int  argCount = randomInt(0, 3);
        bool firstArg = true;
        for (int i = 0; i != argCount; ++i) {
            if (firstArg) { firstArg = false; }
            else {
                ost << ';';
            }
            ost << randomInt(0, 100);
        }

        // csi_dispatch
        ost << randomChar(0x40, 0x7F);
    }

    void writeRandomOsc(std::ostream & ost) {
        ost << ESC << ']';

        int strLength = randomInt(0, 150);
        for (int i = 0; i != strLength; ++i) { ost << randomChar(0x20, 0x80); }

        if (possibility(50)) { ost << '\a'; }
        else {
            ost << ESC << '\\';
        }
    }

    void writeRandomDcs(std::ostream & UNUSED(ost)) {
        // NYI
    }

    void writeRandomSosPmApc(std::ostream & UNUSED(ost)) {
        // NYI
    }

    void writeRandomString(std::ostream & ost) {
        int strLength = randomInt(0, 150);
        for (int i = 0; i != strLength; ++i) { ost << randomChar(0x20, 0x80); }
    }

    void writeRandomBytes(std::ostream & ost) {
        int strLength = randomInt(0, 150);
        for (int i = 0; i != strLength; ++i) { ost << randomChar(0x0, 0x100); }
    }

} // namespace

int main(int argc, char * argv[]) {
    if (argc > 1) {
        auto seed = unstringify<int>(argv[1]);
        ::srandom(seed);
    }

    for (;;) {
        switch (randomInt(0, 9)) {
        case 0: writeRandomControl(std::cout); break;
        case 1: writeRandomEsc(std::cout); break;
        case 2: writeRandomCsi(std::cout); break;
        case 3: writeRandomOsc(std::cout); break;
        case 4: writeRandomDcs(std::cout); break;
        case 5: writeRandomSosPmApc(std::cout); break;
        case 6: writeRandomString(std::cout); break;
        case 7: writeRandomBytes(std::cout); break;
        case 8:
            // resize
            std::cout << ESC << ']' << "667" << '\a';
            break;
        default: FATAL();
        }
    }

    return 0;
}
