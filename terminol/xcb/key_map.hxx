// vi:noai:sw=4

#ifndef KEY_MAP__HXX
#define KEY_MAP__HXX

#include <string>

#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

class KeyMap {
public:
    KeyMap();
    ~KeyMap();

    bool lookup(xkb_keysym_t keySym, uint8_t state,
                bool appKey, bool appCursor, bool crlf, bool numLock,
                std::string & str) const;

protected:
    static void normalise(xkb_keysym_t & keySym);
};

#endif // KEY_MAP__HXX
