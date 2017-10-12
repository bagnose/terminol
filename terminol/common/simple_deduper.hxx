// vi:noai:sw=4
// Copyright © 2013-2014 David Bryant

#ifndef COMMON__SIMPLE_DEDUPER__HXX
#define COMMON__SIMPLE_DEDUPER__HXX

#include "terminol/common/deduper_interface.hxx"

#include <unordered_map>
#include <vector>
#include <mutex>

//
//
//

class SimpleDeduper final : public I_Deduper {
    struct Entry {
        uint32_t             refs;
        uint32_t             length;
        std::vector<uint8_t> bytes;

        Entry(uint32_t length_, std::vector<uint8_t> && bytes_) :
            refs(1), length(length_), bytes(bytes_) {}
    };

    std::unordered_map<Tag, Entry> _entries;
    size_t                         _totalRefs;
    mutable std::mutex             _mutex;

public:
    SimpleDeduper();

    // I_Deduper implementation:

    Tag store(const std::vector<Cell> & cells) override;
    void lookup(Tag tag, std::vector<Cell> & cells) const override;
    void lookupSegment(Tag tag, uint32_t offset, int16_t maxSize,
                       std::vector<Cell> & cells, bool & cont, int16_t & wrap) const override;
    size_t lookupLength(Tag tag) const override;
    void remove(Tag tag) override;

    void getLineStats(uint32_t & uniqueLines, uint32_t & totalLines) const override;
    void getByteStats(size_t & uniqueBytes1, size_t & totalBytes) const override;
    void dump(std::ostream & ost) const override;

protected:
    static Tag makeTag(const std::vector<uint8_t> & bytes);
};

#endif // COMMON__SIMPLE_DEDUPER__HXX
