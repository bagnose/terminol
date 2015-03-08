// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/stream.hxx"

int main() {
    std::vector<uint8_t> bytes;

    OutMemoryStream os(bytes, true);
    InMemoryStream is(bytes);

    return 0;
}
