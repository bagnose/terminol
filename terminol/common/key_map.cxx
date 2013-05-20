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

const KeyMap::Map KeyMap::MAP_NORMAL[] = {
    { XKB_KEY_Left,  1, '[', 'D' },
    { XKB_KEY_Right, 1, '[', 'C' },
    { XKB_KEY_Up,    1, '[', 'A' },
    { XKB_KEY_Down,  1, '[', 'B' },
    { XKB_KEY_Home,  1, '[', 'H' },
    { XKB_KEY_End,   1, '[', 'F' },
    { 0, 0, 0, 0 }
};

const KeyMap::Map KeyMap::MAP_APPLICATION[] = {
    { XKB_KEY_Left,          1, 'O', 'D' },
    { XKB_KEY_Right,         1, 'O', 'C' },
    { XKB_KEY_Up,            1, 'O', 'A' },
    { XKB_KEY_Down,          1, 'O', 'B' },
    { XKB_KEY_Home,          1, 'O', 'H' },
    { XKB_KEY_End,           1, 'O', 'F' },
    { XKB_KEY_KP_Enter,      1, 'O', 'M' },
    { XKB_KEY_KP_Multiply,   1, 'O', 'j' },
    { XKB_KEY_KP_Add,        1, 'O', 'k' },
    { XKB_KEY_KP_Separator,  1, 'O', 'l' },
    { XKB_KEY_KP_Subtract,   1, 'O', 'm' },
    { XKB_KEY_KP_Divide,     1, 'O', 'o' },
    { 0, 0, 0, 0 }
};

KeyMap::KeyMap(uint8_t maskShift_, uint8_t maskAlt_, uint8_t maskControl_) :
    _maskShift(maskShift_),
    _maskAlt(maskAlt_),
    _maskControl(maskControl_) {}

KeyMap::~KeyMap() {}

bool KeyMap::convert(xkb_keysym_t keySym, uint8_t state,
                     bool UNUSED(appKeypad),
                     bool appCursor,
                     bool crOnLf,
                     bool deleteSendsDel,
                     bool altSendsEsc,
                     std::vector<uint8_t> & str) const {
    normalise(keySym);

    switch (keySym) {
        case XKB_KEY_BackSpace:
            if (state & _maskAlt) { str.push_back(ESC); }
            str.push_back(DEL);
            break;

        case XKB_KEY_Tab:
        case XKB_KEY_Linefeed:
        case XKB_KEY_Clear:
        case XKB_KEY_Pause:
        case XKB_KEY_Scroll_Lock:
        case XKB_KEY_Sys_Req:
        case XKB_KEY_Escape:
            str.push_back(keySym & 0x7F);
            break;

        case XKB_KEY_Return:
            str.push_back(CR);
            if (crOnLf) { str.push_back(LF); }
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
            functionKeyResponse('[', 2, state, '~', str);
            break;

        case XKB_KEY_Delete:
            if (deleteSendsDel) { str.push_back(EOT); }
            else { functionKeyResponse('[', 3, state, '~', str); }
            break;

        case XKB_KEY_Page_Up:
            functionKeyResponse('[', 5, state, '~', str);
            break;

        case XKB_KEY_Page_Down:
            functionKeyResponse('[', 6, state, '~', str);
            break;

        case XKB_KEY_F1:
            functionKeyResponse('O', 1, state, 'P', str);
            break;

        case XKB_KEY_F2:
            functionKeyResponse('O', 1, state, 'Q', str);
            break;

        case XKB_KEY_F3:
            functionKeyResponse('O', 1, state, 'R', str);
            break;

        case XKB_KEY_F4:
            functionKeyResponse('O', 1, state, 'S', str);
            break;

        case XKB_KEY_F5:
            functionKeyResponse('[', 15, state, '~', str);
            break;

        case XKB_KEY_F6:
            functionKeyResponse('[', 17, state, '~', str);
            break;

        case XKB_KEY_F7:
            functionKeyResponse('[', 18, state, '~', str);
            break;

        case XKB_KEY_F8:
            functionKeyResponse('[', 19, state, '~', str);
            break;

        case XKB_KEY_F9:
            functionKeyResponse('[', 20, state, '~', str);
            break;

        case XKB_KEY_F10:
            functionKeyResponse('[', 21, state, '~', str);
            break;

        case XKB_KEY_F12:
            functionKeyResponse('[', 24, state, '~', str);
            break;

        default:
            bool convertUtf8 = true;

            // Handle special keys with alternate mappings.
            if (applyKeyMap(appCursor ? MAP_APPLICATION : MAP_NORMAL,
                            keySym, state, str))
            {
                break;
            }

            if (state & _maskControl) {
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

            if (state & _maskAlt) {
                if (altSendsEsc) { str.push_back(ESC); }
                else { keySym = keySym | 0x80; convertUtf8 = false; }
            }

            if ((keySym < 0x80 ) || (!convertUtf8 && keySym < 0x100)) {
                str.push_back(keySym);
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
                    std::copy(buffer, buffer + ret, std::back_inserter(str));
                }
            }

            break;
    }

    return !str.empty();
}

bool KeyMap::isPotent(xkb_keysym_t keySym) const {
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

void KeyMap::normalise(xkb_keysym_t & keySym) {
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

void KeyMap::functionKeyResponse(char escape, int num, uint8_t state, char code,
                                 std::vector<uint8_t> & str) const {
    std::ostringstream ost;

    int modNum = 0;

    if (state & _maskShift)   modNum |= 1;
    if (state & _maskAlt)     modNum |= 2;
    if (state & _maskControl) modNum |= 4;

    if (modNum != 0) {
        ost << ESC << '[' << num << ';' << modNum + 1 << code;
    }
    else if (code != '~') {
        ost << ESC << escape << code;
    }
    else {
        ost << ESC << escape << num << code;
    }

    std::string s = ost.str();
    std::copy(s.begin(), s.end(), std::back_inserter(str));
}

bool KeyMap::applyKeyMap(const Map * mode, xkb_keysym_t sym, uint8_t state,
                         std::vector<uint8_t> & str) const {
    const Map * map;
    int i = 0;

    while (mode[i].sym) {
        map = &mode[i++];
        if (sym == map->sym) {
            functionKeyResponse(map->escape, map->num, state, map->code, str);
            return true;
        }
    }

    return false;
}
