// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/escape.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/debug.hxx"

#include <sys/ioctl.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    struct winsize w;
    ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

    uint16_t rows = w.ws_row;
    uint16_t cols = w.ws_col;

    uint8_t ascii = SPACE;

    std::cout << CsiEsc::SGR(CsiEsc::StockSGR::BOLD);

    for (uint16_t r = 0; r < rows; ++r) {
        for (uint16_t c = 0; c < cols - 3; c += 3) {
            // HVP
            std::cout << ESC << '[' << (r + 1) << ';' << (c + 1) << 'f';

            // char surrounded by two spaces
            std::cout << ' ' << ascii << ' ';

            // force the draw
            std::cout << ESC << ']' << 666 << BEL << std::flush;

            // back-up twice and overwrite
            std::cout << BS << BS << ' ';

            // force the draw
            std::cout << ESC << ']' << 666 << BEL << std::flush;

            ++ascii;
            if (ascii == DEL) { ascii = SPACE; }
        }
    }

    ::sleep(5);

    return 0;
}
