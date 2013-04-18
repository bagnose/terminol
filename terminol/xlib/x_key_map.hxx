// vi:noai:sw=4

#ifndef X_KEY_MAP__HXX
#define X_KEY_MAP__HXX

#include <string>

#include <stdint.h>

#include <X11/X.h>

class X_KeyMap {
public:
    X_KeyMap();
    ~X_KeyMap();

    bool lookup(KeySym keySym, uint8_t state,
                bool appKey, bool appCursor, bool crlf, bool numLock,
                std::string & str) const;
};

#endif // X_KEY_MAP__HXX
