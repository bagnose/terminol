// vi:noai:sw=4

#include "terminol/support/debug.hxx"
#include "terminol/support/escape.hxx"

int main() {
    /*
    std::cout << "The quick brown fox jumped over the lazy dog." << std::endl;
    */

    std::cout
        << "It was a dark and stormy night; the rain fell in torrents — "
        << "except at occasional intervals, when it was checked by a violent gust "
        << "of wind which swept up the streets (for it is in London that our scene lies), "
        << "rattling along the housetops, and fiercely agitating the scanty flame of the "
        << "lamps that struggled against the darkness."
        << std::endl;

    /*
    for (int i = 0; i != 1024; ++i) {
        uint8_t a = i % 256;
        uint8_t b = (i + 128) % 256;

        std::cout
            << CSIEsc::fg24bit(a, a, a)
            << CSIEsc::bg24bit(b, b, b)
            << (i % 10);
    }

    std::cout << std::endl;
    */

    return 0;
}
