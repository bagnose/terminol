// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"

int main(int argc, char * argv[]) {
    size_t columns = 160;

    if (argc > 1) {
        try {
            columns = unstringify<size_t>(argv[1]);
        }
        catch (const ParseError & ex) {
            FATAL("Bad lines: " << ex.message);
        }
    }

    for (size_t i = 0; i != columns; ++i) {
        std::cout << i << std::endl;
    }

    return 0;
}
