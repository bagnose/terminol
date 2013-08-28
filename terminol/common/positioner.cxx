// vi:noai:sw=4

#include "terminol/support/debug.hxx"
#include "terminol/support/escape.hxx"
#include "terminol/support/conv.hxx"

int main(int argc, char *argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <row> <col>" << std::endl;
        return 0;
    }

    try {
        std::cout << MoveCursor(unstringify<int16_t>(argv[1]),
                                unstringify<int16_t>(argv[2]));
        return 0;
    }
    catch (const ParseError & ex) {
        std::cerr << ex.message << std::endl;
        return 1;
    }
}
