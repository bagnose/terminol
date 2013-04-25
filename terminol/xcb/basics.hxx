// vi:noai:sw=4

#ifndef BASICS__HXX
#define BASICS__HXX

#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

class X_Basics : protected Uncopyable {
    xcb_connection_t  * _connection;
    xcb_screen_t      * _screen;
    xcb_visualtype_t  * _visual;
    xcb_key_symbols_t * _keySymbols;

    uint8_t             _numLockMask;
    uint8_t             _shiftLockMask;
    uint8_t             _capsLockMask;
    uint8_t             _modeSwitchMask;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    X_Basics() throw (Error);
    ~X_Basics();

    xcb_connection_t  * connection() { return _connection; }
    xcb_screen_t      * screen()     { return _screen;     }
    xcb_visualtype_t  * visual()     { return _visual;     }
    xcb_key_symbols_t * keySymbols() { return _keySymbols; }

    xcb_keysym_t        getKeySym(xcb_keycode_t keyCode, uint8_t state);

protected:
    void determineMasks();
};

#endif // BASICS__HXX
