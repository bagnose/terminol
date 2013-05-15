// vi:noai:sw=4

#ifndef XCB__KEY_MAP__HXX
#define XCB__KEY_MAP__HXX

#include "terminol/support/pattern.hxx"

#include <vector>

#include <stdint.h>

#include <xkbcommon/xkbcommon.h>

class KeyMap : protected Uncopyable {
    struct Map {
        xkb_keysym_t  sym;
        int           num;
        uint8_t       escape;
        uint8_t       code;
    };

    static const Map MAP_NORMAL[];
    static const Map MAP_APPLICATION[];

    const uint8_t _maskShift;
    const uint8_t _maskAlt;
    const uint8_t _maskControl;

public:
    // shift, alt, control
    KeyMap(uint8_t maskShift, uint8_t maskAlt, uint8_t maskControl);
    ~KeyMap();

    bool convert(xkb_keysym_t keySym, uint8_t state,
                 bool appKey, bool appCursor, bool crlf,
                 std::vector<uint8_t> & str) const;

    bool isPotent(xkb_keysym_t keySym) const;

    uint8_t maskShift()   const { return _maskShift; }
    uint8_t maskAlt()     const { return _maskAlt; }
    uint8_t maskControl() const { return _maskControl; }

protected:
    static void normalise(xkb_keysym_t & keySym);

    void functionKeyResponse(char escape, int num, uint8_t state, char code,
                             std::vector<uint8_t> & str) const;

    bool applyKeyMap(const Map * mode, xkb_keysym_t sym, uint8_t state,
                     std::vector<uint8_t> & str) const;
};

#endif // XCB__KEY_MAP__HXX
