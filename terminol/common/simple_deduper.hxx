// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#ifndef COMMON__SIMPLE_DEDUPER__HXX
#define COMMON__SIMPLE_DEDUPER__HXX

#include "terminol/common/deduper_interface.hxx"

#include <unordered_map>
#include <vector>

//
//
//

class SimpleDeduper : public I_Deduper {
    struct Entry {
        std::vector<Cell> cells;
        uint32_t          refs;

        explicit Entry(const std::vector<Cell> & cells_) : cells(cells_), refs(1) {}
    };

    std::unordered_map<Tag, Entry> _entries;
    size_t                         _totalRefs;

public:
    SimpleDeduper();
    virtual ~SimpleDeduper();

    // I_Deduper implementation:

    Tag store(const std::vector<Cell> & cells) override;
    void lookup(Tag tag, std::vector<Cell> & cells) const override;
    void lookupSegment(Tag tag, uint32_t offset, int16_t maxSize,
                       std::vector<Cell> & cells, bool & cont, int16_t & wrap) const;
    size_t lookupLength(Tag tag) const override;
    void remove(Tag tag) override;

    void getLineStats(uint32_t & uniqueLines, uint32_t & totalLines) const override;
    void getByteStats(size_t & uniqueBytes1, size_t & totalBytes) const override;
    void dump(std::ostream & ost) const override;

protected:
    static Tag makeTag(const std::vector<Cell> & cells);
};

#endif // COMMON__SIMPLE_DEDUPER__HXX
