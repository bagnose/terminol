// vi:noai:sw=4

#ifndef COMMON__DEDUPER__HXX
#define COMMON__DEDUPER__HXX

#include "terminol/common/data_types.hxx"

#include <unordered_map>
#include <algorithm>
#include <vector>
#include <iostream>

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

protected:
    I_Deduper() {}
    ~I_Deduper() {}
};

//
//
//

namespace {

template <class T> struct SDBM {
    typedef T Type;
    T operator () (T val, uint8_t next) {
        return static_cast<T>(next) + (val << 6) + (val << 16) - val;
    }
};

template <class A> typename A::Type hash(const void * buffer,
                                         size_t       length) {
    auto buf = static_cast<const uint8_t *>(buffer);
    return std::accumulate(buf, buf + length, typename A::Type(0), A());
}

} // namespace {anonymous}

//
//
//

class Deduper : public I_Deduper {
    struct Payload {
        Payload(bool cont_, uint16_t wrap_, std::vector<Cell> & cells_) :
            line(cont_, wrap_, cells_),
            refCount(1) {}

        Line     line;
        uint32_t refCount;
    };

    std::unordered_map<Tag, Payload> _lines;
    size_t                           _totalRefCount;

public:
    Deduper() : _lines(), _totalRefCount(0) {}
    virtual ~Deduper() {}

    Tag store(bool cont, uint16_t wrap, std::vector<Cell> & cells) {
        auto tag = makeTag(cells);

again:
        auto iter = _lines.find(tag);

        if (iter == _lines.end()) {
            _lines.insert(std::make_pair(tag, Payload(cont, wrap, cells)));
        }
        else {
            auto & payload = iter->second;

            if (cont  != payload.line.cont ||
                wrap  != payload.line.wrap ||
                cells != payload.line.cells) {
#if DEBUG
                std::cerr << "Collision between:" << std::endl;
                std::cerr << "   '";
                for (auto c : cells) { std::cerr << c.seq; }
                std::cerr << "'" << std::endl;
                std::cerr << "And:" << std::endl;
                std::cerr << "   '";
                for (auto c : payload.line.cells) { std::cerr << c.seq; }
                std::cerr << "'" << std::endl << std::endl;
#endif

                ENFORCE(static_cast<Tag>(_lines.size()) != 0, "No dedupe room left");

                ++tag;
                goto again;
            }

            ++payload.refCount;
        }

        ++_totalRefCount;

#if 0
        if (_totalRefCount != 0) {
            std::cerr << "+++ " << 100.0 * double(_lines.size()) / double(_totalRefCount) << " %" << std::endl;
        }
#endif

        return tag;
    }

    const Line & lookup(Tag tag) const {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        return iter->second.line;
    }

    void remove(Tag tag) {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & payload = iter->second;

        if (--payload.refCount == 0) {
            _lines.erase(iter);
        }

        --_totalRefCount;

#if 0
        if (_totalRefCount != 0) {
            std::cerr << "--- " << 100.0 * double(_lines.size()) / double(_totalRefCount) << " %" << std:endl;
        }
#endif
    }

    void lookupRemove(Tag tag, Line & line) {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & payload = iter->second;

        if (--payload.refCount == 0) {
            line.cont  = payload.line.cont;
            line.wrap  = payload.line.wrap;
            line.cells = std::move(payload.line.cells);
            ASSERT(payload.line.cells.empty(), "");
            _lines.erase(iter);
        }
        else {
            line = payload.line;
        }
    }

private:
    Tag makeTag(const std::vector<Cell> & cells) const {
        return hash<SDBM<Tag>>(&cells.front(), sizeof(Cell) * cells.size());
    }
};

#endif // COMMON__DEDUPER__HXX