// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/common/ascii.hxx"

#include <unistd.h>

int main() {
    std::cout << ESC << '[' << "?25l"; // hide cursor

    const char values[] = {'|', '/', '-', '\\'};

    for (int i = 0;; ++i) {
        auto c = values[i % 4];
        std::cout << '\r' << c << std::flush;
        usleep(50'000);
    }

    std::cout << ESC << '[' << "?25h"; // show cursor

    return 0;
}
