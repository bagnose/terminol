// vi:noai:sw=4

#ifndef COMMON__BINDINGS__HXX
#define COMMON__BINDINGS__HXX

#include "terminol/common/bit_sets.hxx"

#include <xkbcommon/xkbcommon.h>

#include <map>

struct KeyCombo {
    KeyCombo() : key(XKB_KEY_NoSymbol), modifiers() {}
    KeyCombo(xkb_keysym_t key_, ModifierSet modifiers_) :
        key(key_), modifiers(modifiers_) {}

    xkb_keysym_t key;
    ModifierSet  modifiers;
};

inline bool operator < (const KeyCombo & lhs, const KeyCombo & rhs) {
    if (lhs.key < rhs.key) {
        return true;
    }
    else if (rhs.key < lhs.key) {
        return false;
    }
    else {
        return lhs.modifiers.bits() < rhs.modifiers.bits();
    }
}

/*
std::ostream & operator << (std::ostream & ost, const KeyCombo & keyCombo);
*/

//
//
//

enum class Action {
    LOCAL_FONT_RESET,
    LOCAL_FONT_SMALLER,
    LOCAL_FONT_BIGGER,
    GLOBAL_FONT_RESET,
    GLOBAL_FONT_SMALLER,
    GLOBAL_FONT_BIGGER,
    COPY_TO_CLIPBOARD,
    PASTE_FROM_CLIPBOARD,
    SCROLL_UP_ONE_LINE,
    SCROLL_DOWN_ONE_LINE,
    SCROLL_UP_ONE_PAGE,
    SCROLL_DOWN_ONE_PAGE,
    SCROLL_TOP,
    SCROLL_BOTTOM,
    FLOOD_TTY,
    CLEAR_HISTORY,
    DEBUG_GLOBAL_TAGS,
    DEBUG_LOCAL_TAGS,
    DEBUG_HISTORY,
    DEBUG_ACTIVE,
    DEBUG_MODES,
    DEBUG_SELECTION,
    DEBUG_STATS,
    DEBUG_STATS2
};

//std::ostream & operator << (std::ostream & ost, Action action);

//
//
//

typedef std::map<KeyCombo, Action> Bindings;

#endif // COMMON__BINDINGS__HXX
