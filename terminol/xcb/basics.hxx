// vi:noai:sw=4

#ifndef XCB__BASICS__HXX
#define XCB__BASICS__HXX

#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

class Basics : protected Uncopyable {
    xcb_connection_t  * _connection;
    xcb_screen_t      * _screen;
    xcb_visualtype_t  * _visual;
    xcb_key_symbols_t * _keySymbols;

    uint8_t             _maskShift;
    uint8_t             _maskAlt;
    uint8_t             _maskControl;
    uint8_t             _maskNumLock;
    uint8_t             _maskShiftLock;
    uint8_t             _maskCapsLock;
    uint8_t             _maskModeSwitch;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Basics() throw (Error);
    ~Basics();

    xcb_connection_t  * connection() { return _connection; }
    xcb_screen_t      * screen()     { return _screen;     }
    xcb_visualtype_t  * visual()     { return _visual;     }
    xcb_key_symbols_t * keySymbols() { return _keySymbols; }

    xcb_keysym_t        getKeySym(xcb_keycode_t keyCode, uint8_t state);

    uint8_t             maskShift()      const { return _maskShift; }
    uint8_t             maskAlt()        const { return _maskAlt; }
    uint8_t             maskControl()    const { return _maskControl; }
    uint8_t             maskNumLock()    const { return _maskNumLock; }
    uint8_t             maskShiftLock()  const { return _maskShiftLock; }
    uint8_t             maskCapsLock()   const { return _maskCapsLock; }
    uint8_t             maskModeSwitch() const { return _maskModeSwitch; }

    std::string         stateToString(uint8_t state) const;

protected:
    void determineMasks();
};

#endif // XCB__BASICS__HXX
