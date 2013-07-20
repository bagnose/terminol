// vi:noai:sw=4

#include "terminol/support/debug.hxx"
#include "terminol/common/bindings.hxx"

#if 0
std::ostream & operator << (std::ostream & ost, const KeyCombo & keyCombo) {
    ost << keyCombo.modifiers << "-" << keyCombo.key;
    return ost;
}
#endif

/*
std::ostream & operator << (std::ostream & ost, Action action) {
    switch (action) {
    LOCAL_FONT_RESET,
    LOCAL_FONT_BIGGER,
    LOCAL_FONT_SMALLER,
    GLOBAL_FONT_RESET,
    GLOBAL_FONT_BIGGER,
    GLOBAL_FONT_SMALLER,
    COPY_TO_CLIPBOARD,
    PASTE_FROM_CLIPBOARD,
    SCROLL_UP_ONE_LINE,
    SCROLL_DOWN_ONE_LINE,
    SCROLL_UP_ONE_PAGE,
    SCROLL_DOWN_ONE_PAGE,
    SCROLL_TOP,
    SCROLL_BOTTOM,
    DEBUG_1,
    DEBUG_2,
    DEBUG_3
    }

    FATAL("Unreachable");
}
*/
