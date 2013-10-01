// vi:noai:sw=4

#include "terminol/support/regex.hxx"

int main() {
    Regex regex("(\\w+) and (\\w+)");
    auto m = regex.match("Some foods are chewy and crunchy.");
    ENFORCE(m.size() == 3, m.size());
    ENFORCE(m[0] == "chewy and crunchy", m[0]);
    ENFORCE(m[1] == "chewy", m[1]);
    ENFORCE(m[2] == "crunchy", m[2]);

    return 0;
}
