// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/data_types.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"

int main() {
    auto strCol = "#3142BD";
    auto color  = unstringify<Color>(strCol);
    ENFORCE(color.r == 0x31, );
    ENFORCE(color.g == 0x42, );
    ENFORCE(color.b == 0xBD, );
    auto strCol2 = stringify(color);
    ENFORCE(strCol == strCol2, << "Strings don't match: " << strCol << " vs " << strCol2);

    return 0;
}
