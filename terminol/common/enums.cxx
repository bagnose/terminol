// vi:noai:sw=4

#include "terminol/common/enums.hxx"
#include "terminol/common/support.hxx"

std::ostream & operator << (std::ostream & ost, Control control) {
    switch (control) {
        case CONTROL_BEL:
            return ost << "BEL";
        case CONTROL_BS:
            return ost << "BS";
        case CONTROL_HT:
            return ost << "HT";
        case CONTROL_LF:
            return ost << "LF";
        case CONTROL_VT:
            return ost << "VT";
        case CONTROL_FF:
            return ost << "FF";
        case CONTROL_CR:
            return ost << "CR";
        case CONTROL_SO:
            return ost << "SO";
        case CONTROL_SI:
            return ost << "SI";
    }

    FATAL(static_cast<int>(control));
}

std::ostream & operator << (std::ostream & ost, ClearScreen clear) {
    switch (clear) {
        case CLEAR_SCREEN_BELOW:
            return ost << "BELOW";
        case CLEAR_SCREEN_ABOVE:
            return ost << "ABOVE";
        case CLEAR_SCREEN_ALL:
            return ost << "ALL";
    }

    FATAL(static_cast<int>(clear));
}

std::ostream & operator << (std::ostream & ost, ClearLine clear) {
    switch (clear) {
        case CLEAR_LINE_RIGHT:
            return ost << "RIGHT";
        case CLEAR_LINE_LEFT:
            return ost << "LEFT";
        case CLEAR_LINE_ALL:
            return ost << "ALL";
    }

    FATAL(static_cast<int>(clear));
}

std::ostream & operator << (std::ostream & ost, Mode mode) {
    switch (mode) {
        case MODE_WRAP:
            return ost << "WRAP";
        case MODE_INSERT:
            return ost << "INSERT";
        case MODE_APPKEYPAD:
            return ost << "APPKEYPAD";
        case MODE_ALTSCREEN:
            return ost << "ALTSCREEN";
        case MODE_CRLF:
            return ost << "CRLF";
        case MODE_MOUSEBTN:
            return ost << "MOUSEBTN";
        case MODE_MOUSEMOTION:
            return ost << "MOUSEMOTION";
        case MODE_REVERSE:
            return ost << "REVERSE";
        case MODE_KBDLOCK:
            return ost << "KBDLOCK";
        case MODE_HIDE:
            return ost << "HIDE";
        case MODE_ECHO:
            return ost << "ECHO";
        case MODE_APPCURSOR:
            return ost << "APPCURSOR";
        case MODE_MOUSESGR:
            return ost << "MOUSESGR";
    }

    FATAL("Invalid mode: " << static_cast<int>(mode));
}

std::ostream & operator << (std::ostream & ost, Attribute attribute) {
    switch (attribute) {
        case ATTRIBUTE_BOLD:
            return ost << "BOLD";
        case ATTRIBUTE_ITALIC:
            return ost << "ITALIC";
        case ATTRIBUTE_UNDERLINE:
            return ost << "UNDERLINE";
        case ATTRIBUTE_BLINK:
            return ost << "BLINK";
        case ATTRIBUTE_REVERSE:
            return ost << "REVERSE";
    }

    FATAL("Invalid attribute: " << static_cast<int>(attribute));
}
