// vi:noai:sw=4

#ifndef COMMON__DEDUPER_INTERFACE__HXX
#define COMMON__DEDUPER_INTERFACE__HXX

#include "terminol/common/data_types.hxx"

#include <vector>

class I_Deduper {
public:
    typedef uint32_t Tag;

    struct Line {
        Line() : cont(false), wrap(0), cells() {}

        Line(bool cont_, uint16_t wrap_, std::vector<Cell> & cells_) :
            cont(cont_),
            wrap(wrap_),
            cells(std::move(cells_)) {}

        bool              cont;
        uint16_t          wrap;
        std::vector<Cell> cells;
    };

    virtual Tag store(bool cont, uint16_t wrap, std::vector<Cell> & cells) = 0;
    virtual const Line & lookup(Tag tag) const = 0;
    virtual void remove(Tag tag) = 0;
    virtual void lookupRemove(Tag tag, Line & line) = 0;
    virtual double getReduction() const = 0;

protected:
    I_Deduper() {}
    ~I_Deduper() {}
};

#endif // COMMON__DEDUPER_INTERFACE__HXX
