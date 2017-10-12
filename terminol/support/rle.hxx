// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT_RLE_HXX
#define SUPPORT_RLE_HXX

#include "terminol/support/stream.hxx"

#include <vector>
#include <cstdint>
#include <limits>

//
// The maximum number of bytes that can be required is:
// items.size() * (sizeof(Count) + sizeof(Item))
//

template <typename Count = uint8_t, typename Item>
void rleEncode(const std::vector<Item> & items, OutStream & ostream) {
    static_assert(std::is_unsigned<Count>::value, "");
    static_assert(std::is_standard_layout<Item>::value, "");

    auto  initial = true;
    Count count   = 0;
    Item  item;

    for (auto & next : items) {
        if (initial || next != item || count == std::numeric_limits<Count>::max()) {
            if (initial) {
                initial = false;
            }
            else {
                ostream.writeAll(&count, sizeof count, 1);
                ostream.writeAll(&item,  sizeof item,  1);
            }
            item  = next;
            count = 0;
        }

        ++count;
    }

    if (!initial) {
        ostream.writeAll(&count, sizeof count, 1);
        ostream.writeAll(&item,  sizeof item,  1);
        count = 0;
    }

    ostream.writeAll(&count, sizeof count, 1);
}

//
//
//

template <typename Count = uint8_t, typename Item>
void rleDecode(InStream & istream, std::vector<Item> & items) {
    static_assert(std::is_unsigned<Count>::value, "");
    static_assert(std::is_standard_layout<Item>::value, "");

    for (;;) {
        Count count = 0;
        istream.readAll(&count, sizeof count, 1);

        if (count == 0) break;

        Item item;
        istream.readAll(&item, sizeof item, 1);

        for (Count i = 0; i != count; ++i) {
            items.push_back(item);
        }
    }
}

#endif // SUPPORT_RLE_HXX
