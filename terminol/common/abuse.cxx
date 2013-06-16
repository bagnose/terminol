// vi:noai:sw=4

#include "terminol/common/ascii.hxx"
#include "terminol/support/escape.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

#include <cstdlib>

// FIXME model this off the state machine

int randomInt(int min, int max /* exclusive */) {
    return min + (random() % (max - min));
}

uint8_t randomChar(int min, int max /* exclusive */) {
    return static_cast<uint8_t>(randomInt(min, max));
}

bool possibility(int percent) {
    return randomInt(0, 100) < percent;
}

void writeRandomControl(std::ostream & ost) {
again:
    uint8_t c = randomChar(0x0, 0x20);
    if (c == 0x18 || c == 0x1A || c == 0x1B) { goto again; }
    ost << c;
}

void writeRandomEsc(std::ostream & ost) {
    ost << ESC;
again:
    uint8_t c = randomChar(0x30, 0x7F);
    if (c == 0x50 || c == 0x58 || c == 0x5B || c == 0x5D ||
        c == 0x5E || c == 0x5F) { goto again; }
    ost << c;
}

void writeRandomCsi(std::ostream & ost) {
    ost << ESC << '[';

    int argCount = randomInt(0, 3);
    bool firstArg = true;
    for (int i = 0; i != argCount; ++i) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << randomInt(0, 100);
    }

    // csi_dispatch
    ost << randomChar(0x40, 0x7F);
}

void writeRandomOsc(std::ostream & UNUSED(ost)) {
}

void writeRandomDcs(std::ostream & UNUSED(ost)) {
}

void writeRandomSpecial(std::ostream & UNUSED(ost)) {
}

void writeRandomString(std::ostream & ost) {
    int strLength = randomInt(0, 150);
    for (int i = 0; i != strLength; ++i) {
        ost << randomChar(0x20, 0x7F);
    }
}

int main(int argc, char * argv[]) {
    if (argc > 1) {
        int seed = unstringify<int>(argv[1]);
        ::srandom(seed);
    }

    for (;;) {
        switch (randomInt(0, 7)) {
            case 0:
                writeRandomControl(std::cout);
                break;
            case 1:
                writeRandomEsc(std::cout);
                break;
            case 2:
                writeRandomCsi(std::cout);
                break;
            case 3:
                writeRandomOsc(std::cout);
                break;
            case 4:
                writeRandomDcs(std::cout);
                break;
            case 5:
                writeRandomSpecial(std::cout);
                break;
            case 6:
                writeRandomString(std::cout);
                break;
        }
    }

    return 0;
}
