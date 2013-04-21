// vi:noai:sw=4

#ifndef KEY_MAP__HXX
#define KEY_MAP__HXX

#include <string>

#include <stdint.h>

#include <xcb/xcb.h>

class X_KeyMap {
public:
    X_KeyMap();
    ~X_KeyMap();

    bool lookup(xcb_keysym_t keySym, uint8_t state,
                bool appKey, bool appCursor, bool crlf, bool numLock,
                std::string & str) const;
};

#endif // KEY_MAP__HXX
