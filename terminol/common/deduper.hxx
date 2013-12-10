// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__DEDUPER__HXX
#define COMMON__DEDUPER__HXX

#include "terminol/common/deduper_interface.hxx"

#include <unordered_map>
#include <vector>
#include <iostream>

//
//
//

class Deduper : public I_Deduper {
    struct Payload {
        std::vector<Cell> cells;
        uint32_t          refs;

        explicit Payload(std::vector<Cell> & cells_) : cells(std::move(cells_)), refs(1) {}
    };

    std::unordered_map<Tag, Payload> _lines;
    size_t                           _totalRefs;

public:
    Deduper();
    virtual ~Deduper();

    Tag store(std::vector<Cell> & cells);
    const std::vector<Cell> & lookup(Tag tag) const;
    void remove(Tag tag);
    std::vector<Cell> lookupRemove(Tag tag);
    void getStats(uint32_t & uniqueLines, uint32_t & totalLines) const;
    void getStats2(size_t & bytes1, size_t & bytes2) const;
    void dump(std::ostream & ost) const;

protected:
    static Tag makeTag(const std::vector<Cell> & cells);
};

#endif // COMMON__DEDUPER__HXX
