// vi:noai:sw=4

#ifndef X_KEY_MAP__HXX
#define X_KEY_MAP__HXX

#include <string>

#include <stdint.h>

#include <X11/X.h>

class X_KeyMap {
public:
    bool lookup(KeySym keySym, uint8_t state, bool app_key, bool app_cursor,
                std::string & str);
};

#endif // X_KEY_MAP__HXX
