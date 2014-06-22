// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__DEDUPER__HXX
#define COMMON__DEDUPER__HXX

#include "terminol/common/deduper_interface.hxx"

#include <unordered_map>
#include <vector>

//
//
//

class Deduper : public I_Deduper {
    struct Entry {
        std::vector<Cell> cells;
        uint32_t          refs;

        explicit Entry(std::vector<Cell> & cells_) : cells(std::move(cells_)), refs(1) {}
    };

    std::unordered_map<Tag, Entry> _entries;
    size_t                         _totalRefs;

public:
    Deduper();
    virtual ~Deduper();

    // I_Deduper implementation:

    Tag store(std::vector<Cell> & cells) override;
    const std::vector<Cell> & lookup(Tag tag) const override;
    void remove(Tag tag) override;
    std::vector<Cell> lookupRemove(Tag tag) override;
    size_t lookupLength(Tag tag) const override;
    void getLineStats(uint32_t & uniqueLines, uint32_t & totalLines) const override;
    void getByteStats(size_t & uniqueBytes1, size_t & totalBytes) const override;
    void dump(std::ostream & ost) const override;

protected:
    static Tag makeTag(const std::vector<Cell> & cells);
};

#endif // COMMON__DEDUPER__HXX
