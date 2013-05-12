// vi:noai:sw=4

#ifndef XCB__BASICS__HXX
#define XCB__BASICS__HXX

#include "terminol/common/support.hxx"

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

    uint8_t                 maskShift()      const { return _maskShift;       }
    uint8_t                 maskAlt()        const { return _maskAlt;         }
    uint8_t                 maskControl()    const { return _maskControl;     }
    uint8_t                 maskSuper()      const { return _maskSuper;       }
    uint8_t                 maskNumLock()    const { return _maskNumLock;     }
    uint8_t                 maskShiftLock()  const { return _maskShiftLock;   }
    uint8_t                 maskCapsLock()   const { return _maskCapsLock;    }
    uint8_t                 maskModeSwitch() const { return _maskModeSwitch;  }

    xcb_keysym_t            getKeySym(xcb_keycode_t keyCode, uint8_t state);

    std::string             stateToString(uint8_t state) const;

protected:
    void determineMasks();
};

#endif // XCB__BASICS__HXX
