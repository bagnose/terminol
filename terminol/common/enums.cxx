// vi:noai:sw=4

#include "terminol/common/enums.hxx"
#include "terminol/support/debug.hxx"

std::ostream & operator << (std::ostream & ost, Control control) {
    switch (control) {
        case Control::BEL:
            return ost << "BEL";
        case Control::BS:
            return ost << "BS";
        case Control::HT:
            return ost << "HT";
        case Control::LF:
            return ost << "LF";
        case Control::VT:
            return ost << "VT";
        case Control::FF:
            return ost << "FF";
        case Control::CR:
            return ost << "CR";
        case Control::SO:
            return ost << "SO";
        case Control::SI:
            return ost << "SI";
    }

    FATAL(static_cast<int>(control));
}

std::ostream & operator << (std::ostream & ost, Modifier modifier) {
    switch (modifier) {
        case Modifier::SHIFT:
            return ost << "SHIFT";
        case Modifier::ALT:
            return ost << "ALT";
        case Modifier::CONTROL:
            return ost << "CONTROL";
        case Modifier::SUPER:
            return ost << "SUPER";
        case Modifier::NUM_LOCK:
            return ost << "NUM_LOCK";
        case Modifier::SHIFT_LOCK:
            return ost << "SHIFT_LOCK";
        case Modifier::CAPS_LOCK:
            return ost << "CAPS_LOCK";
        case Modifier::MODE_SWITCH:
            return ost << "MODE_SWITCH";
    }

    FATAL(static_cast<int>(modifier));
}

std::ostream & operator << (std::ostream & ost, Mode mode) {
    switch (mode) {
        case Mode::ORIGIN:
            return ost << "ORIGIN";
        case Mode::AUTO_WRAP:
            return ost << "AUTO_WRAP";
        case Mode::AUTO_REPEAT:
            return ost << "AUTO_REPEAT";
        case Mode::SHOW_CURSOR:
            return ost << "SHOW_CURSOR";
        case Mode::ALT_SENDS_ESC:
            return ost << "ALT_SENDS_ESC";
        case Mode::DELETE_SENDS_DEL:
            return ost << "DELETE_SENDS_DEL";
        case Mode::CR_ON_LF:
            return ost << "CR_ON_LF";
        case Mode::INSERT:
            return ost << "INSERT";
        case Mode::ECHO:
            return ost << "ECHO";
        case Mode::KBDLOCK:
            return ost << "KBDLOCK";
        case Mode::APPKEYPAD:
            return ost << "APPKEYPAD";
        case Mode::APPCURSOR:
            return ost << "APPCURSOR";
        case Mode::REVERSE:
            return ost << "REVERSE";
        case Mode::MOUSE_PRESS_RELEASE:
            return ost << "MOUSE_PRESS_RELEASE";
        case Mode::MOUSE_DRAG:
            return ost << "MOUSE_DRAG";
        case Mode::MOUSE_MOTION:
            return ost << "MOUSE_MOTION";
        case Mode::MOUSE_SELECT:
            return ost << "MOUSE_SELECT";
        case Mode::MOUSE_FORMAT_SGR:
            return ost << "MOUSE_FORMAT_SGR";
        case Mode::BRACKETED_PASTE:
            return ost << "BRACKETED_PASTE";
        case Mode::META_8BIT:
            return ost << "META_8BIT";
        case Mode::FOCUS:
            return ost << "FOCUS";
    }

    FATAL("Invalid mode: " << static_cast<int>(mode));
}

std::ostream & operator << (std::ostream & ost, Attr attr) {
    switch (attr) {
        case Attr::BOLD:
            return ost << "BOLD";
        case Attr::FAINT:
            return ost << "FAINT";
        case Attr::ITALIC:
            return ost << "ITALIC";
        case Attr::UNDERLINE:
            return ost << "UNDERLINE";
        case Attr::BLINK:
            return ost << "BLINK";
        case Attr::INVERSE:
            return ost << "INVERSE";
        case Attr::CONCEAL:
            return ost << "CONCEAL";
    }

    FATAL("Invalid attr: " << static_cast<int>(attr));
}

std::ostream & operator << (std::ostream & ost, Hand hand) {
    switch (hand) {
        case Hand::LEFT:
            return ost << "LEFT";
        case Hand::RIGHT:
            return ost << "RIGHT";
    }

    FATAL("Invalid hand: " << static_cast<int>(hand));
}
