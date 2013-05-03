// vi:noai:sw=4

#include "terminol/common/buffer.hxx"

void dump(std::ostream & ost, const Buffer & buffer) {
    for (uint16_t r = 0; r != buffer.getRows(); ++r) {
        ost << r << ": ";
        for (uint16_t c = 0; c != buffer.getCols(); ++c) {
            ost << buffer.getCell(r, c);
        }
        ost << std::endl;
    }
}
