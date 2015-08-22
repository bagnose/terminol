// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__DEDUPER_INTERFACE__HXX
#define COMMON__DEDUPER_INTERFACE__HXX

#include "terminol/common/data_types.hxx"

#include <vector>
#include <numeric>

class I_Deduper {
public:
    using Tag = uint32_t;       // Note, can use smaller tag sizes to cause collisions, etc.
    static Tag invalidTag() { return std::numeric_limits<Tag>::max(); }

    virtual Tag store(const std::vector<Cell> & cells) = 0;
    virtual void lookup(Tag tag, std::vector<Cell> & cells) const = 0;
    virtual void lookupSegment(Tag tag, uint32_t offset, int16_t maxSize,
                               std::vector<Cell> & cells, bool & cont, int16_t & wrap) const = 0;
    virtual size_t lookupLength(Tag tag) const = 0;
    virtual void remove(Tag tag) = 0;

    virtual void getLineStats(uint32_t & uniqueLines, uint32_t & totalLines) const = 0;
    virtual void getByteStats(size_t & uniqueBytes, size_t & totalBytes) const = 0;
    virtual void dump(std::ostream & ost) const = 0;

protected:
    I_Deduper() {}
    ~I_Deduper() {}
};

#endif // COMMON__DEDUPER_INTERFACE__HXX
