// vi:noai:sw=4

#ifndef XCB__KEY_MAP__HXX
#define XCB__KEY_MAP__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/common/bit_sets.hxx"

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

public:
    KeyMap() {}

    bool convert(xkb_keysym_t keySym, ModifierSet modifiers,
                 bool appKeypad,
                 bool appCursor,
                 bool crOnLf,
                 bool deleteSendsDel,
                 bool altSendsEsc,
                 std::vector<uint8_t> & str) const;

    bool isPotent(xkb_keysym_t keySym) const;

protected:
    static void normalise(xkb_keysym_t & keySym);

    void functionKeyResponse(char escape, int num, ModifierSet modifiers, char code,
                             std::vector<uint8_t> & str) const;

    bool applyKeyMap(const Map * mode, xkb_keysym_t sym, ModifierSet modifiers,
                     std::vector<uint8_t> & str) const;
};

#endif // XCB__KEY_MAP__HXX
