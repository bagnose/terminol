// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/data_types.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"

int main() try {
    auto strCol = "#3142BD";
    auto color = unstringify<Color>(strCol);
    ENFORCE(color.r == 0x31, "Red is wrong.");
    ENFORCE(color.g == 0x42, "Green is wrong.");
    ENFORCE(color.b == 0xBD, "Blue is wrong.");
    auto strCol2 = stringify(color);
    ENFORCE(strCol == strCol2, "Strings don't match: " << strCol << " vs " << strCol2);

    return 0;
}
catch (const ParseError & error) {
    FATAL(error.message);
}
