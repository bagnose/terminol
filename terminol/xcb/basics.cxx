// vi:noai:sw=4

#include "terminol/xcb/basics.hxx"
#include "terminol/support/debug.hxx"

#include <xkbcommon/xkbcommon-keysyms.h>

#include <sstream>
#include <cstdlib>

#include <unistd.h>
#include <limits.h>

Basics::Basics() throw (Error) {
    char h[HOST_NAME_MAX + 1];
    if (::gethostname(h, sizeof h) == 0) {
        _hostname = h;
    }

    auto d = ::getenv("DISPLAY");
    _display = d ? d : ":0";

    _connection = xcb_connect(nullptr, &_screenNum);
    if (xcb_connection_has_error(_connection)) {
        throw Error("Failed to connect to display.");
    }
    auto connectionGuard = scopeGuard([&] { xcb_disconnect(_connection); });

    auto setup      = xcb_get_setup(_connection);
    auto screenIter = xcb_setup_roots_iterator(setup);
    for (int i = 0; i != _screenNum; ++i) {
        xcb_screen_next(&screenIter);
    }
    _screen = screenIter.data;

    _visual = nullptr;
    for (xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(_screen);
         depth_iter.rem; xcb_depth_next(&depth_iter))
    {
        for (auto visual_iter = xcb_depth_visuals_iterator(depth_iter.data);
             visual_iter.rem; xcb_visualtype_next(&visual_iter))
        {
            if (_screen->root_visual == visual_iter.data->visual_id) {
                _visual = visual_iter.data;
                break;
            }
        }
    }
    if (!_visual) {
        throw Error("Failed to locate visual");
    }

    _keySymbols = xcb_key_symbols_alloc(_connection);
    if (!_keySymbols) {
        throw Error("Failed to load key symbols");
    }
    auto keySymbolsGuard = scopeGuard([&] { xcb_key_symbols_free(_keySymbols); });

    if (xcb_ewmh_init_atoms_replies(&_ewmhConnection,
                                    xcb_ewmh_init_atoms(_connection, &_ewmhConnection),
                                    nullptr) == 0) {
        throw Error("Failed to initialise EWMH atoms");
    }
    auto ewmhConnectionGuard =
        scopeGuard([&] { xcb_ewmh_connection_wipe(&_ewmhConnection); } );

    _atomPrimary    = XCB_ATOM_PRIMARY;
    _atomClipboard  = lookupAtom("CLIPBOARD");
    _atomUtf8String = lookupAtom("UTF8_STRING");    // XXX fall back on XB_ATOM_STRING
    _atomTargets    = lookupAtom("TARGETS");

    determineMasks();

    keySymbolsGuard.dismiss();
    connectionGuard.dismiss();
}

Basics::~Basics() {
    xcb_ewmh_connection_wipe(&_ewmhConnection);
    xcb_key_symbols_free(_keySymbols);
    xcb_disconnect(_connection);
}

// The following code was stolen and modified from awesome/keyresolv.c

// Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
// Copyright © 2008 Pierre Habouzit <madcoder@debian.org>

bool Basics::getKeySym(xcb_keycode_t keyCode, uint8_t state,
                       xcb_keysym_t & keySym, ModifierSet & modifiers) const {
    modifiers = convertState(state);

    xcb_keysym_t k0, k1;

    /* 'col' (third parameter) is used to get the proper KeySym
     * according to modifier (XCB doesn't provide an equivalent to
     * XLookupString()).
     *
     * If Mode_Switch is ON we look into second group.
     */
    if (modifiers.get(Modifier::MODE_SWITCH)) {
        k0 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 4);
        k1 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 5);
    }
    else {
        k0 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 0);
        k1 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 1);
    }

    /* If the second column does not exists use the first one. */
    if (k1 == XCB_NO_SYMBOL) {
        k1 = k0;
    }

    /* The numlock modifier is on and the second KeySym is a keypad
     * KeySym */
    if (modifiers.get(Modifier::NUM_LOCK) && xcb_is_keypad_key(k1)) {
        // The Shift modifier is on, or if the Lock modifier is on and
        // is interpreted as ShiftLock, use the first KeySym.
        if ((state & XCB_MOD_MASK_SHIFT) ||
            ((state & XCB_MOD_MASK_LOCK) && (state & _maskShiftLock))) {
            keySym = k0;
            return true;
        }
        else {
            keySym = k1;
            return true;
        }
    }
    // The Shift and Lock modifers are both off, use the first KeySym.
    else if (!(state & XCB_MOD_MASK_SHIFT) && !(state & XCB_MOD_MASK_LOCK)) {
        keySym = k0;
        return true;
    }
    // The Shift modifier is off and the Lock modifier is on and is
    // interpreted as CapsLock.
    else if (!(state & XCB_MOD_MASK_SHIFT) &&
             (state & XCB_MOD_MASK_LOCK) && (state & _maskCapsLock)) {
        // The first Keysym is used but if that KeySym is lowercase
        // alphabetic, then the corresponding uppercase KeySym is used
        // instead.
        keySym = k1;
        return true;
    }
    // The Shift modifier is on, and the Lock modifier is on and is
    // interpreted as CapsLock.
    else if ((state & XCB_MOD_MASK_SHIFT) && (state & XCB_MOD_MASK_LOCK) &&
             (state & _maskCapsLock)) {
        // The second Keysym is used but if that KeySym is lowercase
        // alphabetic, then the corresponding uppercase KeySym is used
        // instead.
        keySym = k1;
        return true;
    }
    // The Shift modifier is on, or the Lock modifier is on and is
    // interpreted as ShiftLock, or both.
    else if ((state & XCB_MOD_MASK_SHIFT) ||
             ((state & XCB_MOD_MASK_LOCK) && (state & _maskShiftLock))) {
        keySym = k1;
        return true;
    }

    keySym = XCB_NO_SYMBOL;
    return false;
}

ModifierSet Basics::convertState(uint8_t state) const {
    ModifierSet modifiers;

    if (state & _maskShift)      { modifiers.set(Modifier::SHIFT); }
    if (state & _maskAlt)        { modifiers.set(Modifier::ALT); }
    if (state & _maskControl)    { modifiers.set(Modifier::CONTROL); }
    if (state & _maskSuper)      { modifiers.set(Modifier::SUPER); }
    if (state & _maskNumLock)    { modifiers.set(Modifier::NUM_LOCK); }
    if (state & _maskShiftLock)  { modifiers.set(Modifier::SHIFT_LOCK); }
    if (state & _maskCapsLock)   { modifiers.set(Modifier::CAPS_LOCK); }
    if (state & _maskModeSwitch) { modifiers.set(Modifier::MODE_SWITCH); }

    return modifiers;
}

xcb_atom_t Basics::lookupAtom(const std::string & name) throw (Error) {
    auto cookie = xcb_intern_atom(_connection, 0, name.length(), name.data());
    auto reply  = xcb_intern_atom_reply(_connection, cookie, nullptr);

    if (reply) {
        xcb_atom_t atom = reply->atom;
        std::free(reply);
        return atom;
    }
    else {
        throw Error("Failed to get atom: " + name);
    }
}

void Basics::determineMasks() {
    // Note, xcb_key_symbols_get_keycode() may return nullptr.
    xcb_keycode_t * shiftCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Shift_L);
    xcb_keycode_t * altCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Alt_L);
    xcb_keycode_t * controlCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Control_L);
    xcb_keycode_t * superCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Super_L);
    xcb_keycode_t * numLockCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Num_Lock);
    xcb_keycode_t * shiftLockCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Shift_Lock);
    xcb_keycode_t * capsLockCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Caps_Lock);
    xcb_keycode_t * modeSwitchCodes =
        xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Mode_switch);

    xcb_get_modifier_mapping_cookie_t cookie =
        xcb_get_modifier_mapping_unchecked(_connection);
    xcb_get_modifier_mapping_reply_t * modmapReply =
        xcb_get_modifier_mapping_reply(_connection, cookie, nullptr);
    ASSERT(modmapReply, "");
    xcb_keycode_t * modmap = xcb_get_modifier_mapping_keycodes(modmapReply);
    ASSERT(modmap, "");

    // Clear the masks.
    _maskShift = _maskAlt = _maskControl = _maskSuper =
        _maskNumLock = _maskShiftLock = _maskCapsLock = _maskModeSwitch = 0;

    for (int i = 0; i != 8; ++i) {
        for (int j = 0; j != modmapReply->keycodes_per_modifier; ++j) {
            xcb_keycode_t kc = modmap[i * modmapReply->keycodes_per_modifier + j];

#define LOOK_FOR(mask, codes) \
            if (mask == 0 && codes) { \
                for (xcb_keycode_t * ktest = codes; *ktest; ++ktest) { \
                    if (*ktest == kc) { \
                        mask = (1 << i); \
                        break; \
                    } \
                } \
            }

            LOOK_FOR(_maskShift,      shiftCodes);
            LOOK_FOR(_maskAlt,        altCodes);
            LOOK_FOR(_maskControl,    controlCodes);
            LOOK_FOR(_maskSuper,      superCodes);
            LOOK_FOR(_maskNumLock,    numLockCodes);
            LOOK_FOR(_maskShiftLock,  shiftLockCodes);
            LOOK_FOR(_maskCapsLock,   capsLockCodes);
            LOOK_FOR(_maskModeSwitch, modeSwitchCodes);
#undef LOOK_FOR
        }
    }

    std::free(modmapReply);

    if (modeSwitchCodes) { std::free(modeSwitchCodes); }
    if (capsLockCodes)   { std::free(capsLockCodes); }
    if (shiftLockCodes)  { std::free(shiftLockCodes); }
    if (numLockCodes)    { std::free(numLockCodes); }
    if (superCodes)      { std::free(superCodes); }
    if (controlCodes)    { std::free(controlCodes); }
    if (altCodes)        { std::free(altCodes); }
    if (shiftCodes)      { std::free(shiftCodes); }
}
