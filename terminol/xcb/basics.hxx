// vi:noai:sw=4

#ifndef XCB__BASICS__HXX
#define XCB__BASICS__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/common/bit_sets.hxx"

#include <string>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

class Basics : protected Uncopyable {
    std::string             _hostname;

    xcb_connection_t      * _connection;
    int                     _screenNum;
    xcb_screen_t          * _screen;
    xcb_visualtype_t      * _visual;
    xcb_key_symbols_t     * _keySymbols;

    xcb_ewmh_connection_t   _ewmhConnection;

    uint8_t                 _maskShift;
    uint8_t                 _maskAlt;
    uint8_t                 _maskControl;
    uint8_t                 _maskSuper;
    uint8_t                 _maskNumLock;
    uint8_t                 _maskShiftLock;
    uint8_t                 _maskCapsLock;
    uint8_t                 _maskModeSwitch;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Basics() throw (Error);
    ~Basics();

    const std::string     & hostname()       const { return _hostname;        }

    xcb_connection_t      * connection()           { return _connection;      }
    int                     screenNum()            { return _screenNum;       }
    xcb_screen_t          * screen()               { return _screen;          }
    xcb_visualtype_t      * visual()               { return _visual;          }

    xcb_ewmh_connection_t * ewmhConnection()       { return &_ewmhConnection; }

    // FIXME make these const and return local member data
    xcb_atom_t              atomPrimary() { return XCB_ATOM_PRIMARY; }
    xcb_atom_t              atomClipboard() { return lookupAtom("CLIPBOARD"); }
    xcb_atom_t              atomUtf8String() { return lookupAtom("UTF8_STRING"); }  // TODO fallback on XCB_ATOM_STRING
    xcb_atom_t              atomTargets() { return lookupAtom("TARGETS"); }

    bool                    getKeySym(xcb_keycode_t keyCode, uint8_t state,
                                      xcb_keysym_t & keySym, ModifierSet & modifiers) const;

    ModifierSet             convertState(uint8_t state) const;

    // FIXME don't want this, should use terminol-defined state/mask
    std::string             stateToString(uint8_t state) const;

protected:
    xcb_atom_t lookupAtom(const std::string & name) throw (Error);
    void       determineMasks();
};

#endif // XCB__BASICS__HXX
