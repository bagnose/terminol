// vi:noai:sw=4

#include "terminol/common/bit_sets.hxx"

#include <iostream>

std::ostream & operator << (std::ostream & ost, AttrSet attributeSet) {
    bool first = true;
    for (auto i = 0; i != static_cast<int>(Attr::LAST) + 1; ++i) {
        auto a = static_cast<Attr>(i);
        if (attributeSet.get(a)) {
            if (first) { first = false; }
            else       { ost << "|"; }
            ost << a;
        }
    }
    return ost;
}

std::ostream & operator << (std::ostream & ost, ModeSet modeSet) {
    bool first = true;
    for (auto i = 0; i != static_cast<int>(Mode::LAST) + 1; ++i) {
        auto a = static_cast<Mode>(i);
        if (modeSet.get(a)) {
            if (first) { first = false; }
            else       { ost << "|"; }
            ost << a;
        }
    }
    return ost;
}
