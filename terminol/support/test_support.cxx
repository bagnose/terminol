// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

namespace {

void test1() {
    try {
        ENFORCE(!split("######"), "Comment 1");
        ENFORCE(!split("#"), " Comment 2");
        ENFORCE(!split("\t    # COMMENT"), "Comment 3");
        ENFORCE(!split(""), "Empty line");
        ENFORCE(!split(" \t \t"), "Blank line");

        auto t = split(" \t  a b c_d  foo       \t");
        ENFORCE(t, "");
        auto tokens = *t;
        ENFORCE(tokens.size() == 4, "");
        ENFORCE(tokens[0] == "a", "");
        ENFORCE(tokens[1] == "b", "");
        ENFORCE(tokens[2] == "c_d", "");
        ENFORCE(tokens[3] == "foo", "");

        t = split(" \"a b c\" \"d e f\" \"\" ");
        ENFORCE(t, "");
        tokens = *t;
        ENFORCE(tokens.size() == 3, "");
        ENFORCE(tokens[0] == "a b c", "");
        ENFORCE(tokens[1] == "d e f", "");
        ENFORCE(tokens[2] == "", "");
    }
    catch (const ParseError &) {
        ENFORCE(false, "Unreachable");
    }

    try {
        split("\"unterminated quote...");
        ENFORCE(false, "Unreachable");
    }
    catch (const ParseError &) {
    }
}

void test2(uint8_t num, const char ascii[2]) {
    char    tmpAscii[2];
    byteToHex(num, tmpAscii[0], tmpAscii[1]);
    ENFORCE(tmpAscii[0] == ascii[0] && tmpAscii[1] == ascii[1],
           "Mismatch byte --> hex: " <<
           tmpAscii[0] << tmpAscii[1] << " == " << ascii[0] << ascii[1]);

    try {
        uint8_t tmpNum = hexToByte(ascii[0], ascii[1]);
        ENFORCE(tmpNum == num, "Mismatch hex --> byte: " << tmpNum << " == " << num);
    }
    catch (const ParseError & error) {
        FATAL(error.message);
    }
}

} // namespace {anonymous}

int main() {
    test1();

    test2(0, "00");
    test2(255, "FF");
    test2(127, "7F");

    return 0;
}
