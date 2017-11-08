// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/rle.hxx"
#include "terminol/support/debug.hxx"

int main() {
    std::vector<uint8_t> buffer;
    OutMemoryStream      os(buffer);
    std::vector<int>     input{1, 2, 3, 3};
    rleEncode(input, os);
    InMemoryStream  is(buffer);
    decltype(input) output;
    rleDecode(is, output);

    ENFORCE(input == output, );

    return 0;
}
