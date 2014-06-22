// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__DEDUPER_INTERFACE__HXX
#define COMMON__DEDUPER_INTERFACE__HXX

#include "terminol/common/data_types.hxx"

#include <vector>
#include <numeric>

class I_Deduper {
public:
    typedef uint32_t Tag;       // Note, can use smaller tag sizes to cause collisions, etc.
    static Tag invalidTag() { return std::numeric_limits<Tag>::max(); }

    virtual Tag store(std::vector<Cell> & cells) = 0;           // Clears cells.
    virtual const std::vector<Cell> & lookup(Tag tag) const = 0;
    virtual void remove(Tag tag) = 0;
    virtual std::vector<Cell> lookupRemove(Tag tag) = 0;
    virtual size_t lookupLength(Tag tag) const = 0;
    virtual void getLineStats(uint32_t & uniqueLines, uint32_t & totalLines) const = 0;
    virtual void getByteStats(size_t & uniqueBytes, size_t & totalBytes) const = 0;
    virtual void dump(std::ostream & ost) const = 0;

protected:
    I_Deduper() {}
    ~I_Deduper() {}
};

#endif // COMMON__DEDUPER_INTERFACE__HXX
