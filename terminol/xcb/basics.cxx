// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/common.hxx"
#include "terminol/support/debug.hxx"

#include <xkbcommon/xkbcommon-keysyms.h>

#include <sstream>
#include <cstdlib>
#include <limits>

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
    for (auto i = 0; i != _screenNum; ++i) {
        xcb_screen_next(&screenIter);
    }
    _screen = screenIter.data;

    _visual = nullptr;
    for (auto depthIter = xcb_screen_allowed_depths_iterator(_screen);
         depthIter.rem; xcb_depth_next(&depthIter))
    {
        for (auto visual_iter = xcb_depth_visuals_iterator(depthIter.data);
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

    _normalCursor = loadNormalCursor();
    auto normalCursorGuard = scopeGuard([&] { xcb_free_cursor(_connection, _normalCursor); });

    _invisibleCursor = loadInvisibleCursor();
    auto invisibleCursorGuard = scopeGuard([&] { xcb_free_cursor(_connection, _invisibleCursor); });

    if (xcb_ewmh_init_atoms_replies(&_ewmhConnection,
                                    xcb_ewmh_init_atoms(_connection, &_ewmhConnection),
                                    nullptr) == 0) {
        throw Error("Failed to initialise EWMH atoms");
    }
    auto ewmhConnectionGuard =
        scopeGuard([&] { xcb_ewmh_connection_wipe(&_ewmhConnection); } );

    try {
        _atomPrimary        = XCB_ATOM_PRIMARY;
        _atomClipboard      = lookupAtom("CLIPBOARD", true);
        try {
            _atomUtf8String = lookupAtom("UTF8_STRING", false);
        }
        catch (const NotFoundError & ex) {
            WARNING("No atom UTF8_STRING, falling back on STRING.");
            _atomUtf8String = XCB_ATOM_STRING;
        }
        _atomTargets            = lookupAtom("TARGETS", true);
        _atomWmProtocols        = lookupAtom("WM_PROTOCOLS", false);
        _atomWmDeleteWindow     = lookupAtom("WM_DELETE_WINDOW", true);
        _atomXRootPixmapId      = lookupAtom("_XROOTPMAP_ID", true);
        _atomNetWmWindowOpacity = lookupAtom("_NET_WM_WINDOW_OPACITY", true);           // FIXME need a wrapper method that throws if atom doesn't exist. getAtom() -> lookupAtom()
    }
    catch (const NotFoundError & ex) {
        throw Error(ex.message);
    }

    try {
        _rootPixmap = getRootPixmap(_atomXRootPixmapId);
    }
    catch (const Error & ex) {
        _rootPixmap = XCB_PIXMAP_NONE;
        // XXX disable PseudoTransparency?
    }

    determineMasks();

    ewmhConnectionGuard.dismiss();
    invisibleCursorGuard.dismiss();
    normalCursorGuard.dismiss();
    keySymbolsGuard.dismiss();
    connectionGuard.dismiss();
}

Basics::~Basics() {
    xcb_ewmh_connection_wipe(&_ewmhConnection);
    xcb_free_cursor(_connection, _invisibleCursor);
    xcb_free_cursor(_connection, _normalCursor);
    xcb_key_symbols_free(_keySymbols);
    xcb_disconnect(_connection);
}

// The following function was stolen and modified from awesome/keyresolv.c

// Copyright © 2008-2009 Julien Danjou <julien@danjou.info>
// Copyright © 2008 Pierre Habouzit <madcoder@debian.org>

bool Basics::getKeySym(xcb_keycode_t keyCode, uint8_t state,
                       xcb_keysym_t & keySym, ModifierSet & modifiers) const {
    modifiers = convertState(state);

    xcb_keysym_t k0, k1;

    // 'col' (third parameter) is used to get the proper KeySym
    // according to modifier (XCB doesn't provide an equivalent to
    // XLookupString()).
    //
    // If Mode_Switch is ON we look into second group.
    if (modifiers.get(Modifier::MODE_SWITCH)) {
        k0 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 4);
        k1 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 5);
    }
    else {
        k0 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 0);
        k1 = xcb_key_symbols_get_keysym(_keySymbols, keyCode, 1);
    }

    // If the second column does not exists use the first one.
    if (k1 == XCB_NO_SYMBOL) {
        k1 = k0;
    }

    // The numlock modifier is on and the second KeySym is a keypad KeySym.
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

void Basics::updateRootPixmap() {
    try {
        _rootPixmap = getRootPixmap(_atomXRootPixmapId);
    }
    catch (const Error & ex) {
        ERROR("Failed to get root pixmap: " << ex.message);
    }
}

xcb_atom_t Basics::lookupAtom(const std::string & name,
                              bool create) throw (NotFoundError, Error) {
    auto cookie = xcb_intern_atom(_connection, create ? 0 : 1, name.length(), name.data());
    auto reply  = xcb_intern_atom_reply(_connection, cookie, nullptr);

    if (reply) {
        auto atom = reply->atom;
        std::free(reply);

        if (atom == XCB_ATOM_NONE) {
            ASSERT(!create, "");
            throw NotFoundError("Atom not found: " + name);
        }
        else {
            return atom;
        }
    }
    else {
        throw Error("Failed to get atom: " + name);
    }
}

xcb_cursor_t Basics::loadNormalCursor() throw (Error) {
    uint16_t          cursorId = 152;    // XC_xterm
    const std::string fontName = "cursor";

    // Load the font:

    auto font   = xcb_generate_id(_connection);
    auto cookie = xcb_open_font_checked(_connection,
                                        font,
                                        fontName.size(),
                                        fontName.data());
    if (xcb_request_failed(_connection, cookie, "can't open font")) {
        throw Error("Failed to open font: " + fontName + ".");
    }
    auto guard  = scopeGuard([&] { xcb_close_font(_connection, font); });

    // Create the cursor:
    auto cursor = xcb_generate_id(_connection);
    auto max    = std::numeric_limits<uint16_t>::max();
    xcb_create_glyph_cursor_checked(_connection,
                                    cursor,
                                    font,
                                    font,
                                    cursorId,
                                    cursorId + 1,
                                    0, 0, 0, max / 2, max / 2, max / 2);
    if (xcb_request_failed(_connection, cookie, "couldn't create cursor")) {
        throw Error("Failed to create cursor.");
    }

    return cursor;
}

xcb_cursor_t Basics::loadInvisibleCursor() throw (Error) {
    auto pixmap = xcb_generate_id(_connection);
    auto cookie = xcb_create_pixmap_checked(_connection,
                                            1,
                                            pixmap,
                                            _screen->root,
                                            1, 1);
    if (xcb_request_failed(_connection, cookie, "couldn't create pixmap")) {
        throw Error("Failed to create pixmap.");
    }
    auto guard = scopeGuard([&] { xcb_free_pixmap(_connection, pixmap); });

    auto cursor = xcb_generate_id(_connection);
    cookie = xcb_create_cursor_checked(_connection,
                                       cursor,
                                       pixmap,
                                       pixmap,
                                       0, 0, 0,
                                       0, 0, 0,
                                       1, 1);
    if (xcb_request_failed(_connection, cookie, "couldn't create cursor")) {
        throw Error("Failed to create cursor.");
    }

    return cursor;
}

xcb_pixmap_t Basics::getRootPixmap(xcb_atom_t atom) throw (Error) {
    auto cookie = xcb_get_property(_connection,
                                   0,     // delete
                                   _screen->root,
                                   atom,
                                   XCB_ATOM_PIXMAP,
                                   0,
                                   1);

    auto reply = xcb_get_property_reply(_connection, cookie, nullptr);

    if (!reply) {
        throw Error("Couldn't query root pixmap [1].");
    }

    auto guard = scopeGuard([&] { std::free(reply); });

    if (xcb_get_property_value_length(reply) != sizeof(xcb_pixmap_t)) {
        throw Error("Couldn't query root pixmap [2].");
    }

    auto pixmap = *static_cast<xcb_pixmap_t *>(xcb_get_property_value(reply));

    return pixmap;
}

namespace {

void resolveCode(uint8_t             & mask,
                 const xcb_keycode_t * codes,
                 xcb_keycode_t         kcode,
                 uint8_t               bit) {
    if (mask == 0 && codes) {
        for (auto ktest = codes; *ktest; ++ktest) {
            if (*ktest == kcode) {
                mask = (1 << bit);
                break;
            }
        }
    }
}

} // namespace {anonymous}

void Basics::determineMasks() throw (Error) {
    // Note, xcb_key_symbols_get_keycode() may return nullptr.
    auto shiftCodes      = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Shift_L);
    auto altCodes        = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Alt_L);
    auto controlCodes    = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Control_L);
    auto superCodes      = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Super_L);
    auto numLockCodes    = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Num_Lock);
    auto shiftLockCodes  = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Shift_Lock);
    auto capsLockCodes   = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Caps_Lock);
    auto modeSwitchCodes = xcb_key_symbols_get_keycode(_keySymbols, XKB_KEY_Mode_switch);

    auto guard = scopeGuard([&] {
                            if (modeSwitchCodes) { std::free(modeSwitchCodes); }
                            if (capsLockCodes)   { std::free(capsLockCodes); }
                            if (shiftLockCodes)  { std::free(shiftLockCodes); }
                            if (numLockCodes)    { std::free(numLockCodes); }
                            if (superCodes)      { std::free(superCodes); }
                            if (controlCodes)    { std::free(controlCodes); }
                            if (altCodes)        { std::free(altCodes); }
                            if (shiftCodes)      { std::free(shiftCodes); }
                            });

    auto cookie      = xcb_get_modifier_mapping(_connection);
    auto modmapReply = xcb_get_modifier_mapping_reply(_connection, cookie, nullptr);
    if (!modmapReply) { throw Error("Couldn't determine masks."); }
    auto modmap      = xcb_get_modifier_mapping_keycodes(modmapReply);

    // Clear the masks.
    _maskShift = _maskAlt = _maskControl = _maskSuper =
        _maskNumLock = _maskShiftLock = _maskCapsLock = _maskModeSwitch = 0;

    for (uint8_t i = 0; i != 8; ++i) {
        for (uint8_t j = 0; j != modmapReply->keycodes_per_modifier; ++j) {
            auto kcode = modmap[i * modmapReply->keycodes_per_modifier + j];

            resolveCode(_maskShift,      shiftCodes,      kcode, i);
            resolveCode(_maskAlt,        altCodes,        kcode, i);
            resolveCode(_maskControl,    controlCodes,    kcode, i);
            resolveCode(_maskSuper,      superCodes,      kcode, i);
            resolveCode(_maskNumLock,    numLockCodes,    kcode, i);
            resolveCode(_maskShiftLock,  shiftLockCodes,  kcode, i);
            resolveCode(_maskCapsLock,   capsLockCodes,   kcode, i);
            resolveCode(_maskModeSwitch, modeSwitchCodes, kcode, i);
        }
    }

    std::free(modmapReply);
}
