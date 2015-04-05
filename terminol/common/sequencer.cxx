// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/escape.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

int main(int argc, char * argv[]) {
    int count = 1;

    if (argc == 2) {
        try {
            count = unstringify<int>(argv[1]);
        }
        catch (const ParseError & error) {
            std::cerr << error.message << std::endl;
            return 1;
        }
    }

    const char * str =
        "It was a dark and stormy night; the rain fell in torrents — "
        "except at occasional intervals, when it was checked by a violent gust "
        "of wind which swept up the streets (for it is in London that our scene lies), "
        "rattling along the housetops, and fiercely agitating the scanty flame of the "
        "lamps that struggled against the darkness.";

    for (int i = 0; i != count; ++i) {
        std::cout << (i == 0 ? "" : " ") << str;
    }

    std::cout << std::endl;

    return 0;
}
