// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef XCB__BASICS__HXX
#define XCB__BASICS__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/common/bit_sets.hxx"

#include <string>

#include <xcb/xcb.h>
#include <xcb/xcb_ewmh.h>
#include <xcb/xcb_keysyms.h>

struct NotFoundError {      // FIXME Move to common location.
    explicit NotFoundError(const std::string & message_) : message(message_) {}
    std::string message;
};

class Basics : protected Uncopyable {
    std::string             _hostname;

    std::string             _displayName;
    xcb_connection_t      * _connection;
    int                     _screenNum;
    xcb_screen_t          * _screen;
    xcb_visualtype_t      * _visual;
    xcb_key_symbols_t     * _keySymbols;

    xcb_ewmh_connection_t   _ewmhConnection;

    xcb_cursor_t            _normalCursor;
    xcb_cursor_t            _invisibleCursor;

    xcb_atom_t              _atomPrimary;
    xcb_atom_t              _atomClipboard;
    xcb_atom_t              _atomUtf8String;
    xcb_atom_t              _atomTargets;
    xcb_atom_t              _atomWmProtocols;
    xcb_atom_t              _atomWmDeleteWindow;
    xcb_atom_t              _atomXRootPixmapId;
    xcb_atom_t              _atomESetRootPmapId;
    xcb_atom_t              _atomNetWmWindowOpacity;

    xcb_pixmap_t            _rootPixmap;

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

    Basics() /*throw (Error)*/;
    ~Basics();

    const std::string     & hostname()       const { return _hostname;        }

    const std::string     & displayName()    const { return _displayName;     }
    xcb_connection_t      * connection()           { return _connection;      }
    int                     fd()                   { return xcb_get_file_descriptor(_connection); }
    int                     screenNum()            { return _screenNum;       }
    xcb_screen_t          * screen()               { return _screen;          }
    xcb_visualtype_t      * visual()               { return _visual;          }

    xcb_ewmh_connection_t * ewmhConnection()       { return &_ewmhConnection; }

    xcb_cursor_t            normalCursor()         { return _normalCursor;    }
    xcb_cursor_t            invisibleCursor()      { return _invisibleCursor; }

    xcb_atom_t              atomPrimary()          { return _atomPrimary; }
    xcb_atom_t              atomClipboard()        { return _atomClipboard; }
    xcb_atom_t              atomUtf8String()       { return _atomUtf8String; }
    xcb_atom_t              atomTargets()          { return _atomTargets; }
    xcb_atom_t              atomWmProtocols()      { return _atomWmProtocols; }
    xcb_atom_t              atomWmDeleteWindow()   { return _atomWmDeleteWindow; }
    xcb_atom_t              atomXRootPixmapId()    { return _atomXRootPixmapId; }
    xcb_atom_t              atomNetWmWindowOpacity() { return _atomNetWmWindowOpacity; }

    xcb_pixmap_t            rootPixmap()           { return _rootPixmap; }

    bool                    getKeySym(xcb_keycode_t   keyCode,
                                      uint8_t         state,
                                      xcb_keysym_t  & keySym,
                                      ModifierSet   & modifiers) const;

    ModifierSet             convertState(uint8_t state) const;

    void                    updateRootPixmap();

protected:
    xcb_atom_t   lookupAtom(const std::string & name,
                            bool create) /*throw (NotFoundError, Error)*/;
    xcb_cursor_t loadNormalCursor() /*throw (Error)*/;
    xcb_cursor_t loadInvisibleCursor() /*throw (Error)*/;
    xcb_pixmap_t getRootPixmap(xcb_atom_t atom) /*throw (Error)*/;
    void         determineMasks() /*throw (Error)*/;
};

#endif // XCB__BASICS__HXX
