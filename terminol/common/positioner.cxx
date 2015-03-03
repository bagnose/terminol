// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/escape.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

int main(int argc, char * argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <row> <col>" << std::endl;
        return 0;
    }

    try {
        std::cout << CsiEsc::CUP(unstringify<int>(argv[1]),
                                 unstringify<int>(argv[2]));
        return 0;
    }
    catch (const ParseError & ex) {
        std::cerr << ex.message << std::endl;
        return 1;
    }
}
