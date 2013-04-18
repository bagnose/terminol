// vi:noai:sw=4

#include "terminol/common/cell.hxx"

#include <iostream>

std::ostream & operator << (std::ostream & ost, const Cell & cell) {
    utf8::Length l = utf8::leadLength(cell.bytes()[0]);
    for (size_t i = 0; i != l; ++i) {
        ost << cell.bytes()[i];
    }
    return ost;
}
