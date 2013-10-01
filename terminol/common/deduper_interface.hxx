// vi:noai:sw=4

#ifndef COMMON__DEDUPER_INTERFACE__HXX
#define COMMON__DEDUPER_INTERFACE__HXX

#include "terminol/common/data_types.hxx"

#include <vector>

class I_Deduper {
public:
    typedef uint32_t Tag;       // Note, can use smaller tag sizes to cause collisions, etc.
    static Tag invalidTag() { return static_cast<Tag>(-1); }

    virtual Tag store(std::vector<Cell> & cells) = 0;           // Clears cells.
    virtual const std::vector<Cell> & lookup(Tag tag) const = 0;
    virtual void remove(Tag tag) = 0;
    virtual std::vector<Cell> lookupRemove(Tag tag) = 0;
    virtual void getStats(uint32_t & uniqueLines, uint32_t & totalLines) const = 0;
    virtual void getStats2(size_t & bytes1, size_t & bytes2) const = 0;
    virtual void dump(std::ostream & ost) const = 0;

protected:
    I_Deduper() {}
    ~I_Deduper() {}
};

#endif // COMMON__DEDUPER_INTERFACE__HXX
