// vi:noai:sw=4

#include "terminol/simple_buffer.hxx"

void dump(std::ostream & ost, const SimpleBuffer & buffer) {
    for (uint16_t r = 0; r != buffer.getRows(); ++r) {
        ost << r << ": ";
        for (uint16_t c = 0; c != buffer.getCols(); ++c) {
            const Char & ch = buffer.getChar(r, c);
            ost << ch;
        }
        ost << std::endl;
    }
}
