// vi:noai:sw=4

#include "terminol/xcb/key_map.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/common/support.hxx"

#include <cstring>

#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-keysyms.h>

namespace {

const uint8_t NO_MASK  = 0;
const uint8_t ANY_MASK = ~NO_MASK;

bool match(uint8_t mask, uint8_t state) {
    if (mask == NO_MASK) {
        return state == 0;
    }
    else if (mask == ANY_MASK) {
        return true;
    }
    else {
        return (state & mask) == mask;
    }
}

struct Key {
    xcb_keysym_t keySym;
    uint8_t      mask;
    const char * str;
    // three valued logic variables: 0 indifferent, 1 on, -1 off
    int8_t       appKey;     // application keypad
    int8_t       appCursor;  // application cursor
    int8_t       crlf;       // crlf mode
};

const Key keys[] = { //                                                appKey  appCursor
    // keySym                mask                        string        keypad   cursor   crlf
    { XKB_KEY_KP_Home,       XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2H",     0,      0,      0 },
    { XKB_KEY_KP_Home,       ANY_MASK,                   "\033[H",        0,     -1,      0 },
    { XKB_KEY_KP_Home,       ANY_MASK,                   "\033[1~",       0,     +1,      0 },
    { XKB_KEY_KP_Up,         ANY_MASK,                   "\033Ox",       +1,      0,      0 },
    { XKB_KEY_KP_Up,         ANY_MASK,                   "\033[A",        0,     -1,      0 },
    { XKB_KEY_KP_Up,         ANY_MASK,                   "\033OA",        0,     +1,      0 },
    { XKB_KEY_KP_Down,       ANY_MASK,                   "\033Or",       +1,      0,      0 },
    { XKB_KEY_KP_Down,       ANY_MASK,                   "\033[B",        0,     -1,      0 },
    { XKB_KEY_KP_Down,       ANY_MASK,                   "\033OB",        0,     +1,      0 },
    { XKB_KEY_KP_Left,       ANY_MASK,                   "\033Ot",       +1,      0,      0 },
    { XKB_KEY_KP_Left,       ANY_MASK,                   "\033[D",        0,     -1,      0 },
    { XKB_KEY_KP_Left,       ANY_MASK,                   "\033OD",        0,     +1,      0 },
    { XKB_KEY_KP_Right,      ANY_MASK,                   "\033Ov",       +1,      0,      0 },
    { XKB_KEY_KP_Right,      ANY_MASK,                   "\033[C",        0,     -1,      0 },
    { XKB_KEY_KP_Right,      ANY_MASK,                   "\033OC",        0,     +1,      0 },
    { XKB_KEY_KP_Prior,      XCB_KEY_BUT_MASK_SHIFT,     "\033[5;2~",     0,      0,      0 },
    { XKB_KEY_KP_Prior,      ANY_MASK,                   "\033[5~",       0,      0,      0 },
    { XKB_KEY_KP_Begin,      ANY_MASK,                   "\033[E",        0,      0,      0 },
    { XKB_KEY_KP_End,        XCB_KEY_BUT_MASK_CONTROL,   "\033[J",       -1,      0,      0 },
    { XKB_KEY_KP_End,        XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5F",    +1,      0,      0 },
    { XKB_KEY_KP_End,        XCB_KEY_BUT_MASK_SHIFT,     "\033[K",       -1,      0,      0 },
    { XKB_KEY_KP_End,        XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2F",    +1,      0,      0 },
    { XKB_KEY_KP_End,        ANY_MASK,                   "\033[4~",       0,      0,      0 },
    { XKB_KEY_KP_Next,       XCB_KEY_BUT_MASK_SHIFT,     "\033[6;2~",     0,      0,      0 },
    { XKB_KEY_KP_Next,       ANY_MASK,                   "\033[6~",       0,      0,      0 },
    { XKB_KEY_KP_Insert,     XCB_KEY_BUT_MASK_SHIFT,     "\033[2;2~",    +1,      0,      0 },
    { XKB_KEY_KP_Insert,     XCB_KEY_BUT_MASK_SHIFT,     "\033[4l",      -1,      0,      0 },
    { XKB_KEY_KP_Insert,     XCB_KEY_BUT_MASK_CONTROL,   "\033[L",       -1,      0,      0 },
    { XKB_KEY_KP_Insert,     XCB_KEY_BUT_MASK_CONTROL,   "\033[2;5~",    +1,      0,      0 },
    { XKB_KEY_KP_Insert,     ANY_MASK,                   "\033[4h",      -1,      0,      0 },
    { XKB_KEY_KP_Insert,     ANY_MASK,                   "\033[2~",      +1,      0,      0 },
    { XKB_KEY_KP_Delete,     XCB_KEY_BUT_MASK_CONTROL,   "\033[2J",      -1,      0,      0 },
    { XKB_KEY_KP_Delete,     XCB_KEY_BUT_MASK_CONTROL,   "\033[3;5~",    +1,      0,      0 },
    { XKB_KEY_KP_Delete,     XCB_KEY_BUT_MASK_SHIFT,     "\033[2K",      +1,      0,      0 },
    { XKB_KEY_KP_Delete,     XCB_KEY_BUT_MASK_SHIFT,     "\033[3;2~",    -1,      0,      0 },
    { XKB_KEY_KP_Delete,     ANY_MASK,                   "\033[P",       -1,      0,      0 },
    { XKB_KEY_KP_Delete,     ANY_MASK,                   "\033[3~",      +1,      0,      0 },
    { XKB_KEY_KP_Multiply,   ANY_MASK,                   "\033Oj",       +2,      0,      0 },
    { XKB_KEY_KP_Add,        ANY_MASK,                   "\033Ok",       +2,      0,      0 },
    { XKB_KEY_KP_Enter,      ANY_MASK,                   "\033OM",       +2,      0,      0 },
    { XKB_KEY_KP_Enter,      ANY_MASK,                   "\r",           -1,      0,     -1 },
    { XKB_KEY_KP_Enter,      ANY_MASK,                   "\r\n",         -1,      0,     +1 },
    { XKB_KEY_KP_Subtract,   ANY_MASK,                   "\033Om",       +2,      0,      0 },
    { XKB_KEY_KP_Decimal,    ANY_MASK,                   "\033On",       +2,      0,      0 },
    { XKB_KEY_KP_Divide,     ANY_MASK,                   "\033Oo",       +2,      0,      0 },
    { XKB_KEY_KP_0,          ANY_MASK,                   "\033Op",       +2,      0,      0 },
    { XKB_KEY_KP_1,          ANY_MASK,                   "\033Oq",       +2,      0,      0 },
    { XKB_KEY_KP_2,          ANY_MASK,                   "\033Or",       +2,      0,      0 },
    { XKB_KEY_KP_3,          ANY_MASK,                   "\033Os",       +2,      0,      0 },
    { XKB_KEY_KP_4,          ANY_MASK,                   "\033Ot",       +2,      0,      0 },
    { XKB_KEY_KP_5,          ANY_MASK,                   "\033Ou",       +2,      0,      0 },
    { XKB_KEY_KP_6,          ANY_MASK,                   "\033Ov",       +2,      0,      0 },
    { XKB_KEY_KP_7,          ANY_MASK,                   "\033Ow",       +2,      0,      0 },
    { XKB_KEY_KP_8,          ANY_MASK,                   "\033Ox",       +2,      0,      0 },
    { XKB_KEY_KP_9,          ANY_MASK,                   "\033Oy",       +2,      0,      0 },
    { XKB_KEY_BackSpace,     NO_MASK,                    "\177",          0,      0,      0 },
    { XKB_KEY_Up,            XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2A",     0,      0,      0 },
    { XKB_KEY_Up,            XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5A",     0,      0,      0 },
    { XKB_KEY_Up,            XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3A",     0,      0,      0 },
    { XKB_KEY_Up,            ANY_MASK,                   "\033[A",        0,     -1,      0 },
    { XKB_KEY_Up,            ANY_MASK,                   "\033OA",        0,     +1,      0 },
    { XKB_KEY_Down,          XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2B",     0,      0,      0 },
    { XKB_KEY_Down,          XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5B",     0,      0,      0 },
    { XKB_KEY_Down,          XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3B",     0,      0,      0 },
    { XKB_KEY_Down,          ANY_MASK,                   "\033[B",        0,     -1,      0 },
    { XKB_KEY_Down,          ANY_MASK,                   "\033OB",        0,     +1,      0 },
    { XKB_KEY_Left,          XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2D",     0,      0,      0 },
    { XKB_KEY_Left,          XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5D",     0,      0,      0 },
    { XKB_KEY_Left,          XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3D",     0,      0,      0 },
    { XKB_KEY_Left,          ANY_MASK,                   "\033[D",        0,     -1,      0 },
    { XKB_KEY_Left,          ANY_MASK,                   "\033OD",        0,     +1,      0 },
    { XKB_KEY_Right,         XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2C",     0,      0,      0 },
    { XKB_KEY_Right,         XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5C",     0,      0,      0 },
    { XKB_KEY_Right,         XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3C",     0,      0,      0 },
    { XKB_KEY_Right,         ANY_MASK,                   "\033[C",        0,     -1,      0 },
    { XKB_KEY_Right,         ANY_MASK,                   "\033OC",        0,     +1,      0 },
    { XKB_KEY_ISO_Left_Tab,  XCB_KEY_BUT_MASK_SHIFT,     "\033[Z",        0,      0,      0 },
    { XKB_KEY_Return,        XCB_KEY_BUT_MASK_MOD_1,     "\033\r",        0,      0,     -1 },
    { XKB_KEY_Return,        XCB_KEY_BUT_MASK_MOD_1,     "\033\r\n",      0,      0,     +1 },
    { XKB_KEY_Return,        ANY_MASK,                   "\r",            0,      0,     -1 },
    { XKB_KEY_Return,        ANY_MASK,                   "\r\n",          0,      0,     +1 },
    { XKB_KEY_Insert,        XCB_KEY_BUT_MASK_SHIFT,     "\033[4l",      -1,      0,      0 },
    { XKB_KEY_Insert,        XCB_KEY_BUT_MASK_SHIFT,     "\033[2;2~",    +1,      0,      0 },
    { XKB_KEY_Insert,        XCB_KEY_BUT_MASK_CONTROL,   "\033[L",       -1,      0,      0 },
    { XKB_KEY_Insert,        XCB_KEY_BUT_MASK_CONTROL,   "\033[2;5~",    +1,      0,      0 },
    { XKB_KEY_Insert,        ANY_MASK,                   "\033[4h",      -1,      0,      0 },
    { XKB_KEY_Insert,        ANY_MASK,                   "\033[2~",      +1,      0,      0 },
    { XKB_KEY_Delete,        XCB_KEY_BUT_MASK_CONTROL,   "\033[2J",      -1,      0,      0 },
    { XKB_KEY_Delete,        XCB_KEY_BUT_MASK_CONTROL,   "\033[3;5~",    +1,      0,      0 },
    { XKB_KEY_Delete,        XCB_KEY_BUT_MASK_SHIFT,     "\033[2K",      +1,      0,      0 },
    { XKB_KEY_Delete,        XCB_KEY_BUT_MASK_SHIFT,     "\033[3;2~",    -1,      0,      0 },
    { XKB_KEY_Delete,        ANY_MASK,                   "\033[P",       -1,      0,      0 },
    { XKB_KEY_Delete,        ANY_MASK,                   "\033[3~",      +1,      0,      0 },
    { XKB_KEY_Home,          XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2H",     0,      0,      0 },
    { XKB_KEY_Home,          ANY_MASK,                   "\033[H",        0,     -1,      0 },
    { XKB_KEY_Home,          ANY_MASK,                   "\033[1~",       0,     +1,      0 },
    { XKB_KEY_End,           XCB_KEY_BUT_MASK_CONTROL,   "\033[J",       -1,      0,      0 },
    { XKB_KEY_End,           XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5F",    +1,      0,      0 },
    { XKB_KEY_End,           XCB_KEY_BUT_MASK_SHIFT,     "\033[K",       -1,      0,      0 },
    { XKB_KEY_End,           XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2F",    +1,      0,      0 },
    { XKB_KEY_End,           ANY_MASK,                   "\033[4~",       0,      0,      0 },
    { XKB_KEY_Prior,         XCB_KEY_BUT_MASK_CONTROL,   "\033[5;5~",     0,      0,      0 },
    { XKB_KEY_Prior,         XCB_KEY_BUT_MASK_SHIFT,     "\033[5;2~",     0,      0,      0 },
    { XKB_KEY_Prior,         NO_MASK,                    "\033[5~",       0,      0,      0 },
    { XKB_KEY_Next,          XCB_KEY_BUT_MASK_CONTROL,   "\033[6;5~",     0,      0,      0 },
    { XKB_KEY_Next,          XCB_KEY_BUT_MASK_SHIFT,     "\033[6;2~",     0,      0,      0 },
    { XKB_KEY_Next,          ANY_MASK,                   "\033[6~",       0,      0,      0 },
    { XKB_KEY_F1,            NO_MASK,                    "\033OP",        0,      0,      0 },
    { XKB_KEY_F1,  /* F13 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2P",     0,      0,      0 },
    { XKB_KEY_F1,  /* F25 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5P",     0,      0,      0 },
    { XKB_KEY_F1,  /* F37 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[1;6P",     0,      0,      0 },
    { XKB_KEY_F1,  /* F49 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3P",     0,      0,      0 },
    { XKB_KEY_F1,  /* F61 */ XCB_KEY_BUT_MASK_MOD_3,     "\033[1;4P",     0,      0,      0 },
    { XKB_KEY_F2,            NO_MASK,                    "\033OQ",        0,      0,      0 },
    { XKB_KEY_F2,  /* F14 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2Q",     0,      0,      0 },
    { XKB_KEY_F2,  /* F26 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5Q",     0,      0,      0 },
    { XKB_KEY_F2,  /* F38 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[1;6Q",     0,      0,      0 },
    { XKB_KEY_F2,  /* F50 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3Q",     0,      0,      0 },
    { XKB_KEY_F2,  /* F62 */ XCB_KEY_BUT_MASK_MOD_3,     "\033[1;4Q",     0,      0,      0 },
    { XKB_KEY_F3,            NO_MASK,                    "\033OR",        0,      0,      0 },
    { XKB_KEY_F3,  /* F15 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2R",     0,      0,      0 },
    { XKB_KEY_F3,  /* F27 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[1;5R",     0,      0,      0 },
    { XKB_KEY_F3,  /* F39 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[1;6R",     0,      0,      0 },
    { XKB_KEY_F3,  /* F51 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3R",     0,      0,      0 },
    { XKB_KEY_F3,  /* F63 */ XCB_KEY_BUT_MASK_MOD_3,     "\033[1;4R",     0,      0,      0 },
    { XKB_KEY_F4,            NO_MASK,                    "\033OS",        0,      0,      0 },
    { XKB_KEY_F4,  /* F16 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[1;2S",     0,      0,      0 },
    { XKB_KEY_F4,  /* F28 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[1;5S",     0,      0,      0 },
    { XKB_KEY_F4,  /* F40 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[1;6S",     0,      0,      0 },
    { XKB_KEY_F4,  /* F52 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[1;3S",     0,      0,      0 },
    { XKB_KEY_F5,            NO_MASK,                    "\033[15~",      0,      0,      0 },
    { XKB_KEY_F5,  /* F17 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[15;2~",    0,      0,      0 },
    { XKB_KEY_F5,  /* F29 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[15;5~",    0,      0,      0 },
    { XKB_KEY_F5,  /* F41 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[15;6~",    0,      0,      0 },
    { XKB_KEY_F5,  /* F53 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[15;3~",    0,      0,      0 },
    { XKB_KEY_F6,            NO_MASK,                    "\033[17~",      0,      0,      0 },
    { XKB_KEY_F6,  /* F18 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[17;2~",    0,      0,      0 },
    { XKB_KEY_F6,  /* F30 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[17;5~",    0,      0,      0 },
    { XKB_KEY_F6,  /* F42 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[17;6~",    0,      0,      0 },
    { XKB_KEY_F6,  /* F54 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[17;3~",    0,      0,      0 },
    { XKB_KEY_F7,            NO_MASK,                    "\033[18~",      0,      0,      0 },
    { XKB_KEY_F7,  /* F19 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[18;2~",    0,      0,      0 },
    { XKB_KEY_F7,  /* F31 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[18;5~",    0,      0,      0 },
    { XKB_KEY_F7,  /* F43 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[18;6~",    0,      0,      0 },
    { XKB_KEY_F7,  /* F55 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[18;3~",    0,      0,      0 },
    { XKB_KEY_F8,            NO_MASK,                    "\033[19~",      0,      0,      0 },
    { XKB_KEY_F8,  /* F20 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[19;2~",    0,      0,      0 },
    { XKB_KEY_F8,  /* F32 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[19;5~",    0,      0,      0 },
    { XKB_KEY_F8,  /* F44 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[19;6~",    0,      0,      0 },
    { XKB_KEY_F8,  /* F56 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[19;3~",    0,      0,      0 },
    { XKB_KEY_F9,            NO_MASK,                    "\033[20~",      0,      0,      0 },
    { XKB_KEY_F9,  /* F21 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[20;2~",    0,      0,      0 },
    { XKB_KEY_F9,  /* F33 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[20;5~",    0,      0,      0 },
    { XKB_KEY_F9,  /* F45 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[20;6~",    0,      0,      0 },
    { XKB_KEY_F9,  /* F57 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[20;3~",    0,      0,      0 },
    { XKB_KEY_F10,           NO_MASK,                    "\033[21~",      0,      0,      0 },
    { XKB_KEY_F10, /* F22 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[21;2~",    0,      0,      0 },
    { XKB_KEY_F10, /* F34 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[21;5~",    0,      0,      0 },
    { XKB_KEY_F10, /* F46 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[21;6~",    0,      0,      0 },
    { XKB_KEY_F10, /* F58 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[21;3~",    0,      0,      0 },
    { XKB_KEY_F11,           NO_MASK,                    "\033[23~",      0,      0,      0 },
    { XKB_KEY_F11, /* F23 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[23;2~",    0,      0,      0 },
    { XKB_KEY_F11, /* F35 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[23;5~",    0,      0,      0 },
    { XKB_KEY_F11, /* F47 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[23;6~",    0,      0,      0 },
    { XKB_KEY_F11, /* F59 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[23;3~",    0,      0,      0 },
    { XKB_KEY_F12,           NO_MASK,                    "\033[24~",      0,      0,      0 },
    { XKB_KEY_F12, /* F24 */ XCB_KEY_BUT_MASK_SHIFT,     "\033[24;2~",    0,      0,      0 },
    { XKB_KEY_F12, /* F36 */ XCB_KEY_BUT_MASK_CONTROL,   "\033[24;5~",    0,      0,      0 },
    { XKB_KEY_F12, /* F48 */ XCB_KEY_BUT_MASK_MOD_4,     "\033[24;6~",    0,      0,      0 },
    { XKB_KEY_F12, /* F60 */ XCB_KEY_BUT_MASK_MOD_1,     "\033[24;3~",    0,      0,      0 },
    { XKB_KEY_F13,           NO_MASK,                    "\033[1;2P",     0,      0,      0 },
    { XKB_KEY_F14,           NO_MASK,                    "\033[1;2Q",     0,      0,      0 },
    { XKB_KEY_F15,           NO_MASK,                    "\033[1;2R",     0,      0,      0 },
    { XKB_KEY_F16,           NO_MASK,                    "\033[1;2S",     0,      0,      0 },
    { XKB_KEY_F17,           NO_MASK,                    "\033[15;2~",    0,      0,      0 },
    { XKB_KEY_F18,           NO_MASK,                    "\033[17;2~",    0,      0,      0 },
    { XKB_KEY_F19,           NO_MASK,                    "\033[18;2~",    0,      0,      0 },
    { XKB_KEY_F20,           NO_MASK,                    "\033[19;2~",    0,      0,      0 },
    { XKB_KEY_F21,           NO_MASK,                    "\033[20;2~",    0,      0,      0 },
    { XKB_KEY_F22,           NO_MASK,                    "\033[21;2~",    0,      0,      0 },
    { XKB_KEY_F23,           NO_MASK,                    "\033[23;2~",    0,      0,      0 },
    { XKB_KEY_F24,           NO_MASK,                    "\033[24;2~",    0,      0,      0 },
    { XKB_KEY_F25,           NO_MASK,                    "\033[1;5P",     0,      0,      0 },
    { XKB_KEY_F26,           NO_MASK,                    "\033[1;5Q",     0,      0,      0 },
    { XKB_KEY_F27,           NO_MASK,                    "\033[1;5R",     0,      0,      0 },
    { XKB_KEY_F28,           NO_MASK,                    "\033[1;5S",     0,      0,      0 },
    { XKB_KEY_F29,           NO_MASK,                    "\033[15;5~",    0,      0,      0 },
    { XKB_KEY_F30,           NO_MASK,                    "\033[17;5~",    0,      0,      0 },
    { XKB_KEY_F31,           NO_MASK,                    "\033[18;5~",    0,      0,      0 },
    { XKB_KEY_F32,           NO_MASK,                    "\033[19;5~",    0,      0,      0 },
    { XKB_KEY_F33,           NO_MASK,                    "\033[20;5~",    0,      0,      0 },
    { XKB_KEY_F34,           NO_MASK,                    "\033[21;5~",    0,      0,      0 },
    { XKB_KEY_F35,           NO_MASK,                    "\033[23;5~",    0,      0,      0 }
};

const size_t numKeys = sizeof(keys) / sizeof(keys[0]);

} // namespace {anonymous}

X_KeyMap::X_KeyMap() {}

X_KeyMap::~X_KeyMap() {}

bool X_KeyMap::lookup(xcb_keysym_t keySym, uint8_t state,
                      bool appKey, bool appCursor, bool crlf, bool numLock,
                      std::string & str) const {
    /*
    PRINT("keySym=" << keySym << ", state=" << int(state) << ", appKey=" << appKey
          << ", appCursor=" << appCursor << ", crlf=" << crlf << ", numLock=" << numLock
          << ", str=" << Str(str)
          << std::endl);
          */

    for (size_t i = 0; i != numKeys; ++i) {
        const Key & key = keys[i];

        if (keySym != key.keySym)            { continue; }
        if (!match(key.mask, state))         { continue; }

        if (key.appKey < 0 && appKey)        { continue; }
        if (key.appKey > 0 && !appKey)       { continue; }
        if (key.appKey == 2 && numLock)      { continue; }

        if (key.appCursor < 0 &&  appCursor) { continue; }
        if (key.appCursor > 0 && !appCursor) { continue; }

        if (key.crlf < 0 &&  crlf)           { continue; }
        if (key.crlf > 0 && !crlf)           { continue; }

        PRINT("Replacing: '" << Str(str) << "' with: '" << Str(key.str) << "'");
        str = key.str;
        return true;
    }

#if 0
    char buffer[16];
    int i = xkb_keysym_to_utf8(keySym, buffer, 16);
    PRINT("Got: i=" << i << ", buffer=" << buffer);
#endif

    return false;
}
