// vi:noai:sw=4

#include "terminol/common/data_types.hxx"

std::ostream & operator << (std::ostream & ost, Color color) {
    ost << '#'
        << nibbleToHex((color.r >> 4) & 0x0F)
        << nibbleToHex((color.r)      & 0x0F)
        << nibbleToHex((color.g >> 4) & 0x0F)
        << nibbleToHex((color.g)      & 0x0F)
        << nibbleToHex((color.b >> 4) & 0x0F)
        << nibbleToHex((color.b)      & 0x0F);
    return ost;
}

std::istream & operator >> (std::istream & ist, Color & color) {
    char str[8];
    ist.get(str, 8);

    if (!ist.good()) {
        return ist;
    }

    if (str[0] != '#') {
        ist.setstate(std::ios::failbit);
        return ist;
    }


    color = Color(hexToByte(str[1], str[2]),
                  hexToByte(str[3], str[4]),
                  hexToByte(str[5], str[6]));

    return ist;
}
