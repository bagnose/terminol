// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/common/bindings.hxx"
#include "terminol/common/key_map.hxx"

std::ostream & operator << (std::ostream & ost, const KeyCombo & keyCombo) {
    ost << keyCombo.modifiers << "-" << xkb::symToName(keyCombo.key);
    return ost;
}

std::ostream & operator << (std::ostream & ost, Action action) {
    switch (action) {
        case Action::WINDOW_NARROWER:
            return ost << "WINDOW_NARROWER";
        case Action::WINDOW_WIDER:
            return ost << "WINDOW_WIDER";
        case Action::WINDOW_SHORTER:
            return ost << "WINDOW_SHORTER";
        case Action::WINDOW_TALLER:
            return ost << "WINDOW_TALLER";
        case Action::LOCAL_FONT_RESET:
            return ost << "LOCAL_FONT_RESET";
        case Action::LOCAL_FONT_SMALLER:
            return ost << "LOCAL_FONT_SMALLER";
        case Action::LOCAL_FONT_BIGGER:
            return ost << "LOCAL_FONT_BIGGER";
        case Action::GLOBAL_FONT_RESET:
            return ost << "GLOBAL_FONT_RESET";
        case Action::GLOBAL_FONT_SMALLER:
            return ost << "GLOBAL_FONT_SMALLER";
        case Action::GLOBAL_FONT_BIGGER:
            return ost << "GLOBAL_FONT_BIGGER";
        case Action::COPY_TO_CLIPBOARD:
            return ost << "COPY_TO_CLIPBOARD";
        case Action::PASTE_FROM_CLIPBOARD:
            return ost << "PASTE_FROM_CLIPBOARD";
        case Action::SCROLL_UP_LINE:
            return ost << "SCROLL_UP_LINE";
        case Action::SCROLL_DOWN_LINE:
            return ost << "SCROLL_DOWN_LINE";
        case Action::SCROLL_UP_PAGE:
            return ost << "SCROLL_UP_PAGE";
        case Action::SCROLL_DOWN_PAGE:
            return ost << "SCROLL_DOWN_PAGE";
        case Action::SCROLL_TOP:
            return ost << "SCROLL_TOP";
        case Action::SCROLL_BOTTOM:
            return ost << "SCROLL_BOTTOM";
        case Action::CLEAR_HISTORY:
            return ost << "CLEAR_HISTORY";
        case Action::DEBUG_GLOBAL_TAGS:
            return ost << "DEBUG_GLOBAL_TAGS";
        case Action::DEBUG_LOCAL_TAGS:
            return ost << "DEBUG_LOCAL_TAGS";
        case Action::DEBUG_HISTORY:
            return ost << "DEBUG_HISTORY";
        case Action::DEBUG_ACTIVE:
            return ost << "DEBUG_ACTIVE";
        case Action::DEBUG_MODES:
            return ost << "DEBUG_MODES";
        case Action::DEBUG_SELECTION:
            return ost << "DEBUG_SELECTION";
        case Action::DEBUG_STATS:
            return ost << "DEBUG_STATS";
        case Action::DEBUG_STATS2:
            return ost << "DEBUG_STATS2";
    }

    FATAL("Invalid action: " << static_cast<int>(action));
}
