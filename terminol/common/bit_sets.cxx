// vi:noai:sw=4

#include "terminol/common/bit_sets.hxx"
#include "terminol/common/support.hxx"

std::ostream & operator << (std::ostream & ost, AttributeSet attributeSet) {
    bool first = true;
    for (auto i = 0; i != LAST_ATTRIBUTE + 1; ++i) {
        auto a = static_cast<Attribute>(i);
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
    for (auto i = 0; i != LAST_MODE + 1; ++i) {
        auto a = static_cast<Mode>(i);
        if (modeSet.get(a)) {
            if (first) { first = false; }
            else       { ost << "|"; }
            ost << a;
        }
    }
    return ost;
}
