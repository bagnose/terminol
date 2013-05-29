// vi:noai:sw=4

#include "terminol/common/bit_sets.hxx"

#include <iostream>

std::ostream & operator << (std::ostream & ost, ModifierSet modifiers) {
    bool first = true;
    for (auto i = 0; i != static_cast<int>(Modifier::LAST) + 1; ++i) {
        auto a = static_cast<Modifier>(i);
        if (modifiers.get(a)) {
            if (first) { first = false; }
            else       { ost << "|"; }
            ost << a;
        }
    }
    return ost;
}

std::ostream & operator << (std::ostream & ost, AttrSet attrs) {
    bool first = true;
    for (auto i = 0; i != static_cast<int>(Attr::LAST) + 1; ++i) {
        auto a = static_cast<Attr>(i);
        if (attrs.get(a)) {
            if (first) { first = false; }
            else       { ost << "|"; }
            ost << a;
        }
    }
    return ost;
}

std::ostream & operator << (std::ostream & ost, ModeSet modes) {
    bool first = true;
    for (auto i = 0; i != static_cast<int>(Mode::LAST) + 1; ++i) {
        auto a = static_cast<Mode>(i);
        if (modes.get(a)) {
            if (first) { first = false; }
            else       { ost << "|"; }
            ost << a;
        }
    }
    return ost;
}
