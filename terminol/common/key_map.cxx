// vi:noai:sw=4

#include "terminol/common/key_map.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/debug.hxx"

#include <sstream>
#include <cstring>

#include <xcb/xcb.h>

// FIXME see 'Key Codes' in man 7 urxvt.

// The following code was stolen and modified from weston/clients/terminal.c.

// Copyright © 2008 Kristian Høgsberg

namespace xkb {

namespace {

struct Map {
    xkb_keysym_t  sym;
    int           num;
    uint8_t       escape;
    uint8_t       code;
};

const Map MAP_NORMAL[] = {
    { XKB_KEY_Left,  1, '[', 'D' },
    { XKB_KEY_Right, 1, '[', 'C' },
    { XKB_KEY_Up,    1, '[', 'A' },
    { XKB_KEY_Down,  1, '[', 'B' },
    { XKB_KEY_Home,  1, '[', 'H' },
    { XKB_KEY_End,   1, '[', 'F' },
    { 0, 0, 0, 0 }
};

const Map MAP_APPLICATION[] = {
    { XKB_KEY_Left,         1, 'O', 'D' },
    { XKB_KEY_Right,        1, 'O', 'C' },
    { XKB_KEY_Up,           1, 'O', 'A' },
    { XKB_KEY_Down,         1, 'O', 'B' },
    { XKB_KEY_Home,         1, 'O', 'H' },
    { XKB_KEY_End,          1, 'O', 'F' },
    { XKB_KEY_KP_Enter,     1, 'O', 'M' },
    { XKB_KEY_KP_Multiply,  1, 'O', 'j' },
    { XKB_KEY_KP_Add,       1, 'O', 'k' },
    { XKB_KEY_KP_Separator, 1, 'O', 'l' },
    { XKB_KEY_KP_Subtract,  1, 'O', 'm' },
    { XKB_KEY_KP_Divide,    1, 'O', 'o' },
    { 0, 0, 0, 0 }
};

void normalise(xkb_keysym_t & keySym) {
    switch (keySym) {
        case XKB_KEY_KP_Space:
            keySym = XKB_KEY_space;
            break;
        case XKB_KEY_KP_Tab:
            keySym = XKB_KEY_Tab;
            break;
        case XKB_KEY_KP_Enter:
            keySym = XKB_KEY_Return;
            break;
        case XKB_KEY_KP_Left:
            keySym = XKB_KEY_Left;
            break;
        case XKB_KEY_KP_Up:
            keySym = XKB_KEY_Up;
            break;
        case XKB_KEY_KP_Right:
            keySym = XKB_KEY_Right;
            break;
        case XKB_KEY_KP_Down:
            keySym = XKB_KEY_Down;
            break;
        case XKB_KEY_KP_Equal:
            keySym = XKB_KEY_equal;
            break;
        case XKB_KEY_KP_Multiply:
            keySym = XKB_KEY_asterisk;
            break;
        case XKB_KEY_KP_Add:
            keySym = XKB_KEY_plus;
            break;
        case XKB_KEY_KP_Separator:
            // Note this is actually locale-dependent and should mostly be
            // a comma.  But leave it as period until we one day start
            // doing the right thing.
            keySym = XKB_KEY_period;
            break;
        case XKB_KEY_KP_Subtract:
            keySym = XKB_KEY_minus;
            break;
        case XKB_KEY_KP_Decimal:
            keySym = XKB_KEY_period;
            break;
        case XKB_KEY_KP_Divide:
            keySym = XKB_KEY_slash;
            break;
        case XKB_KEY_KP_0:
        case XKB_KEY_KP_1:
        case XKB_KEY_KP_2:
        case XKB_KEY_KP_3:
        case XKB_KEY_KP_4:
        case XKB_KEY_KP_5:
        case XKB_KEY_KP_6:
        case XKB_KEY_KP_7:
        case XKB_KEY_KP_8:
        case XKB_KEY_KP_9:
            keySym = (keySym - XKB_KEY_KP_0) + XKB_KEY_0;
            break;
        default:
            break;
    }
}

void functionKeyResponse(char escape,
                         int num,
                         ModifierSet modifiers,
                         char code,
                         std::vector<uint8_t> & str) {
    std::ostringstream ost;

    int modNum = 0;

    if (modifiers.get(Modifier::SHIFT))   modNum |= 1;
    if (modifiers.get(Modifier::ALT))     modNum |= 2;
    if (modifiers.get(Modifier::CONTROL)) modNum |= 4;

    if (modNum != 0) {
        ost << ESC << '[' << num << ';' << modNum + 1 << code;
    }
    else if (code != '~') {
        ost << ESC << escape << code;
    }
    else {
        ost << ESC << escape << num << code;
    }

    const std::string & s = ost.str();
    std::copy(s.begin(), s.end(), std::back_inserter(str));
}

bool applyKeyMap(const Map * mode,
                 xkb_keysym_t sym,
                 ModifierSet modifiers,
                 std::vector<uint8_t> & str) {
    for (const Map * map = mode; map->sym; ++map) {
        if (sym == map->sym) {
            functionKeyResponse(map->escape, map->num, modifiers, map->code, str);
            return true;
        }
    }

    return false;
}

} // namespace {anonymous}

std::string symToName(xkb_keysym_t keySym) {
    char buf[128];
    auto size = xkb_keysym_get_name(keySym, buf, sizeof buf);
    ENFORCE(size != -1, "Bad keysym");
    return std::string(buf, buf + size);
}

xkb_keysym_t nameToSym(const std::string & name) throw (ParseError) {
    auto keySym = xkb_keysym_from_name(name.c_str(), static_cast<xkb_keysym_flags>(0));
    if (keySym == XKB_KEY_NoSymbol) {
        throw ParseError("Bad keysym: '" + name + "'");
    }
    else {
        return keySym;
    }
}

bool composeInput(xkb_keysym_t keySym, ModifierSet modifiers,
                  bool UNUSED(appKeypad),
                  bool appCursor,
                  bool crOnLf,
                  bool deleteSendsDel,
                  bool altSendsEsc,
                  std::vector<uint8_t> & input) {
    normalise(keySym);

    switch (keySym) {
        case XKB_KEY_BackSpace:
            if (modifiers.get(Modifier::ALT)) { input.push_back(ESC); }
            input.push_back(DEL);
            break;

        case XKB_KEY_Tab:
        case XKB_KEY_Linefeed:
        case XKB_KEY_Clear:
        case XKB_KEY_Pause:
        case XKB_KEY_Scroll_Lock:
        case XKB_KEY_Sys_Req:
        case XKB_KEY_Escape:
            input.push_back(keySym & 0x7F);
            break;

        case XKB_KEY_Return:
            input.push_back(CR);
            if (crOnLf) { input.push_back(LF); }
            break;

        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:
        case XKB_KEY_Hyper_L:
        case XKB_KEY_Hyper_R:
            break;

        case XKB_KEY_Insert:
            functionKeyResponse('[', 2, modifiers, '~', input);
            break;

        case XKB_KEY_Delete:
            if (deleteSendsDel) { input.push_back(EOT); }
            else { functionKeyResponse('[', 3, modifiers, '~', input); }
            break;

        case XKB_KEY_Page_Up:
            functionKeyResponse('[', 5, modifiers, '~', input);
            break;

        case XKB_KEY_Page_Down:
            functionKeyResponse('[', 6, modifiers, '~', input);
            break;

        case XKB_KEY_F1:
            functionKeyResponse('O', 1, modifiers, 'P', input);
            break;

        case XKB_KEY_F2:
            functionKeyResponse('O', 1, modifiers, 'Q', input);
            break;

        case XKB_KEY_F3:
            functionKeyResponse('O', 1, modifiers, 'R', input);
            break;

        case XKB_KEY_F4:
            functionKeyResponse('O', 1, modifiers, 'S', input);
            break;

        case XKB_KEY_F5:
            functionKeyResponse('[', 15, modifiers, '~', input);
            break;

        case XKB_KEY_F6:
            functionKeyResponse('[', 17, modifiers, '~', input);
            break;

        case XKB_KEY_F7:
            functionKeyResponse('[', 18, modifiers, '~', input);
            break;

        case XKB_KEY_F8:
            functionKeyResponse('[', 19, modifiers, '~', input);
            break;

        case XKB_KEY_F9:
            functionKeyResponse('[', 20, modifiers, '~', input);
            break;

        case XKB_KEY_F10:
            functionKeyResponse('[', 21, modifiers, '~', input);
            break;

        case XKB_KEY_F12:
            functionKeyResponse('[', 24, modifiers, '~', input);
            break;

        default:
            bool convertUtf8 = true;

            // Handle special keys with alternate mappings.
            if (applyKeyMap(appCursor ? MAP_APPLICATION : MAP_NORMAL,
                            keySym, modifiers, input))
            {
                break;
            }

            if (modifiers.get(Modifier::CONTROL)) {
                if (keySym >= '3' && keySym <= '7') {
                    keySym = (keySym & 0x1f) + 8;
                }

                if (!((keySym >= '!' && keySym <= '/') ||
                      (keySym >= '8' && keySym <= '?') ||
                      (keySym >= '0' && keySym <= '2'))) { keySym = keySym & 0x1f; }
                else if (keySym == '2')                  { keySym = 0x00;          }
                else if (keySym == '/')                  { keySym = 0x1F;          }
                else if (keySym == '8' || keySym == '?') { keySym = 0x7F;          }
            }

            if (modifiers.get(Modifier::ALT)) {
                if (altSendsEsc) { input.push_back(ESC); }
                else { keySym = keySym | 0x80; convertUtf8 = false; }
            }

            if ((keySym < 0x80 ) || (!convertUtf8 && keySym < 0x100)) {
                input.push_back(keySym);
            }
            else {
                char buffer[16];
                int ret = xkb_keysym_to_utf8(keySym, buffer, sizeof(buffer));

                if (ret == -1) {
                    ERROR("Buffer to small to encode UTF-8 character.");
                }
                else if (ret == 0) {
                    ERROR("No conversion for key press");
                }
                else {
                    std::copy(buffer, buffer + ret, std::back_inserter(input));
                }
            }

            break;
    }

    return !input.empty();
}

bool isPotent(xkb_keysym_t keySym) {
    normalise(keySym);

    switch (keySym) {
        case XKB_KEY_Shift_L:
        case XKB_KEY_Shift_R:
        case XKB_KEY_Control_L:
        case XKB_KEY_Control_R:
        case XKB_KEY_Alt_L:
        case XKB_KEY_Alt_R:
        case XKB_KEY_Meta_L:
        case XKB_KEY_Meta_R:
        case XKB_KEY_Super_L:
        case XKB_KEY_Super_R:
        case XKB_KEY_Hyper_L:
        case XKB_KEY_Hyper_R:
            return false;
        default:
            return true;
    }
}

} // namespace xkb
