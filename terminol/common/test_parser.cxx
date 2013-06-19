// vi:noai:sw=4

#include "terminol/common/parser.hxx"
#include "terminol/support/debug.hxx"

int main() {
    std::vector<std::string> tokens;

    ENFORCE(!split("######", tokens), "Comment 1");
    ENFORCE(!split("#", tokens), " Comment 2");
    ENFORCE(!split("\t    # COMMENT", tokens), "Comment 3");
    ENFORCE(!split("", tokens), "Empty line");
    ENFORCE(!split(" \t \t", tokens), "Blank line");

    ENFORCE(split(" \t  a b c_d  foo       \t", tokens), "");
    ENFORCE(tokens.size() == 4, "");
    ENFORCE(tokens[0] == "a", "");
    ENFORCE(tokens[1] == "b", "");
    ENFORCE(tokens[2] == "c_d", "");
    ENFORCE(tokens[3] == "foo", "");

    tokens.clear();
    ENFORCE(split(" \"a b c\" \"d e f\" \"\" ", tokens), "");
    ENFORCE(tokens.size() == 3, "");
    ENFORCE(tokens[0] == "a b c", "");
    ENFORCE(tokens[1] == "d e f", "");
    ENFORCE(tokens[2] == "", "");

    try {
        tokens.clear();
        split("\"unterminated quote...", tokens);
        ENFORCE(false, "Unreachable");
    }
    catch (const ParseError &) {
    }

    return 0;
}
