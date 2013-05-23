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

std::ostream & operator << (std::ostream & ost, ClearScreen clear) {
    switch (clear) {
        case ClearScreen::BELOW:
            return ost << "BELOW";
        case ClearScreen::ABOVE:
            return ost << "ABOVE";
        case ClearScreen::ALL:
            return ost << "ALL";
    }

    FATAL(static_cast<int>(clear));
}

std::ostream & operator << (std::ostream & ost, ClearLine clear) {
    switch (clear) {
        case ClearLine::RIGHT:
            return ost << "RIGHT";
        case ClearLine::LEFT:
            return ost << "LEFT";
        case ClearLine::ALL:
            return ost << "ALL";
    }

    FATAL(static_cast<int>(clear));
}

std::ostream & operator << (std::ostream & ost, Mode mode) {
    switch (mode) {
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

        // Remainder are dubious...
        case Mode::MOUSEBTN:
            return ost << "MOUSEBTN";
        case Mode::MOUSEMOTION:
            return ost << "MOUSEMOTION";
        case Mode::MOUSESGR:
            return ost << "MOUSESGR";
    }

    FATAL("Invalid mode: " << static_cast<int>(mode));
}

std::ostream & operator << (std::ostream & ost, Attr attr) {
    switch (attr) {
        case Attr::BOLD:
            return ost << "BOLD";
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
