// vi:noai:sw=4

#ifndef XCB__KEY_MAP__HXX
#define XCB__KEY_MAP__HXX

#include <string>

#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

class KeyMap {
    struct Map {
        xkb_keysym_t  sym;
        int           num;
        char          escape;
        char          code;
    };

    const uint8_t _maskShift;
    const uint8_t _maskAlt;
    const uint8_t _maskControl;

    static const Map MAP_NORMAL[];
    static const Map MAP_APPLICATION[];

public:
    // shift, alt, control
    KeyMap(uint8_t maskShift, uint8_t maskAlt, uint8_t maskControl);
    ~KeyMap();

    bool lookup(xkb_keysym_t keySym, uint8_t state,
                bool appKey, bool appCursor, bool crlf, bool numLock,
                std::string & str) const;

protected:
    static void normalise(xkb_keysym_t & keySym);

    void function_key_response(char escape, int num, uint8_t state, char code,
                               std::ostream & response) const;

    bool apply_key_map(const Map * mode, xkb_keysym_t sym, uint8_t state,
                       std::ostream & response) const;
};

#endif // XCB__KEY_MAP__HXX
