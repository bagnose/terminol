// vi:noai:sw=4

#include "terminol/xcb/key_map.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/common/support.hxx"

#include <cstring>

#include <xcb/xcb.h>

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

KeyMap::KeyMap(uint8_t maskShift, uint8_t maskAlt, uint8_t maskControl) :
    _maskShift(maskShift),
    _maskAlt(maskAlt),
    _maskControl(maskControl) {}

KeyMap::~KeyMap() {}

bool KeyMap::lookup(xkb_keysym_t keySym, uint8_t state,
                    bool UNUSED(appKey),  // Mode::APPKEYPAD - DECPAM
                    bool appCursor,       // Mode::APPCURSOR - DECCKM
                    bool crlf,
                    std::string & str) const {
    normalise(keySym);

    std::ostringstream ost;

    switch (keySym) {
        case XKB_KEY_BackSpace:
            if (state & _maskAlt) {
                ost << ESC;
            }
            ost << DEL;
            break;

        case XKB_KEY_Tab:
        case XKB_KEY_Linefeed:
        case XKB_KEY_Clear:
        case XKB_KEY_Pause:
        case XKB_KEY_Scroll_Lock:
        case XKB_KEY_Sys_Req:
        case XKB_KEY_Escape:
            ost << static_cast<char>(keySym & 0x7F);
            break;

        case XKB_KEY_Return:
            if (crlf /* terminal->mode & MODE_LF_NEWLINE */) {
                ost << CR << LF;
            }
            else {
                ost << CR;
            }
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
            function_key_response('[', 2, state, '~', ost);
            break;

        case XKB_KEY_Delete:
            if (false /*terminal->mode & MODE_DELETE_SENDS_DEL */) {
                ost << EOT;
            }
            else {
                function_key_response('[', 3, state, '~', ost);
            }
            break;

        case XKB_KEY_Page_Up:
            function_key_response('[', 5, state, '~', ost);
            break;

        case XKB_KEY_Page_Down:
            function_key_response('[', 6, state, '~', ost);
            break;

        case XKB_KEY_F1:
            function_key_response('O', 1, state, 'P', ost);
            break;

        case XKB_KEY_F2:
            function_key_response('O', 1, state, 'Q', ost);
            break;

        case XKB_KEY_F3:
            function_key_response('O', 1, state, 'R', ost);
            break;

        case XKB_KEY_F4:
            function_key_response('O', 1, state, 'S', ost);
            break;

        case XKB_KEY_F5:
            function_key_response('[', 15, state, '~', ost);
            break;

        case XKB_KEY_F6:
            function_key_response('[', 17, state, '~', ost);
            break;

        case XKB_KEY_F7:
            function_key_response('[', 18, state, '~', ost);
            break;

        case XKB_KEY_F8:
            function_key_response('[', 19, state, '~', ost);
            break;

        case XKB_KEY_F9:
            function_key_response('[', 20, state, '~', ost);
            break;

        case XKB_KEY_F10:
            function_key_response('[', 21, state, '~', ost);
            break;

        case XKB_KEY_F12:
            function_key_response('[', 24, state, '~', ost);
            break;

        default:
            bool convert_utf8 = true;

            // Handle special keys with alternate mappings.
            if (apply_key_map(appCursor ? MAP_APPLICATION : MAP_NORMAL,
                              keySym, state, ost))
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
                if (false /* terminal->mode & MODE_ALT_SENDS_ESC */) {
                    ost << ESC;
                }
                else {
                    keySym = keySym | 0x80;
                    convert_utf8 = false;
                }
            }

            if ((keySym < 0x80 ) || (!convert_utf8 && keySym < 0x100)) {
                ost << static_cast<char>(keySym);       // char?? uint8_t?
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
                    ost << buffer;
                }
            }

            break;
    }

    str = ost.str();
    return !str.empty();
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
            /* Note this is actually locale-dependent and should mostly be
             * a comma.  But leave it as period until we one day start
             * doing the right thing. */
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

void KeyMap::function_key_response(char escape, int num, uint8_t state, char code,
                                   std::ostream & response) const {
    int mod_num = 0;

    if (state & _maskShift)   mod_num |= 1;
    if (state & _maskAlt)     mod_num |= 2;
    if (state & _maskControl) mod_num |= 4;

    if (mod_num != 0) {
        response << ESC << '[' << num << ';' << mod_num + 1 << code;
    }
    else if (code != '~') {
        response << ESC << escape << code;
    }
    else {
        response << ESC << escape << num << code;
    }
}

bool KeyMap::apply_key_map(const Map * mode, xkb_keysym_t sym, uint8_t state,
                           std::ostream & response) const {
    const Map * map;
    int i = 0;

    while (mode[i].sym) {
        map = &mode[i++];
        if (sym == map->sym) {
            function_key_response(map->escape, map->num, state, map->code, response);
            return true;
        }
    }

    return false;
}
