// vi:noai:sw=4

#ifndef COMMON__DEDUPER__HXX
#define COMMON__DEDUPER__HXX

#include "terminol/common/deduper_interface.hxx"
#include "terminol/support/escape.hxx"

#include <unordered_map>
#include <algorithm>
#include <vector>
#include <iostream>
#include <iomanip>

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
        std::vector<Cell> cells;
        uint32_t          refs;

        explicit Payload(std::vector<Cell> & cells_) : cells(std::move(cells_)), refs(1) {}
    };

    std::unordered_map<Tag, Payload> _lines;
    size_t                           _totalRefs;

public:
    Deduper() : _lines(), _totalRefs(0) {}
    virtual ~Deduper() {}

    Tag store(std::vector<Cell> & cells) {
        auto tag = makeTag(cells);
        ASSERT(tag != invalidTag(), "");

again:
        auto iter = _lines.find(tag);

        if (iter == _lines.end()) {
            _lines.insert(std::make_pair(tag, Payload(cells)));
        }
        else {
            auto & payload = iter->second;

            if (cells != payload.cells) {
#if 0
                std::cerr << "Hash collision:" << std::endl;

                std::cerr << "  \'";
                for (auto & c : cells) {
                    std::cerr << c.seq;
                }
                std::cerr << "\'" << std::endl;

                std::cerr << "  \'";
                for (auto & c : payload.cells) {
                    std::cerr << c.seq;
                }
                std::cerr << "\'" << std::endl;
#endif

                ENFORCE(static_cast<Tag>(_lines.size()) != invalidTag(), "No dedupe room left.");

                ++tag;
                if (tag == invalidTag()) { ++tag; }
                goto again;
            }

            cells.clear();
            ++payload.refs;
        }

        ++_totalRefs;

        ASSERT(cells.empty(), "");

        return tag;
    }

    const std::vector<Cell> & lookup(Tag tag) const {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        return iter->second.cells;
    }

    void remove(Tag tag) {
        ASSERT(tag != invalidTag(), "");
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & payload = iter->second;

        if (--payload.refs == 0) {
            _lines.erase(iter);
        }

        --_totalRefs;
    }

    std::vector<Cell> lookupRemove(Tag tag) {
        ASSERT(tag != invalidTag(), "");
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & payload = iter->second;

        --_totalRefs;

        if (--payload.refs == 0) {
            auto rval = std::move(payload.cells);
            ASSERT(payload.cells.empty(), "");
            _lines.erase(iter);
            return rval;
        }
        else {
            return payload.cells;
        }
    }

    void getStats(uint32_t & uniqueLines, uint32_t & totalLines) const {
        uniqueLines = _lines.size();
        totalLines  = _totalRefs;
    }

    void getStats2(size_t & bytes1, size_t & bytes2) const {
        bytes1 = 0;
        bytes2 = 0;

        for (auto & l : _lines) {
            auto & payload = l.second;

            size_t size = payload.cells.size() * sizeof(Cell);

            bytes1 += size;
            bytes2 += payload.refs * size;
        }
    }

    void dump(std::ostream & ost) const {
        ost << "BEGIN GLOBAL TAGS" << std::endl;

        size_t i = 0;

        for (auto & l : _lines) {
            auto   tag     = l.first;
            auto & payload = l.second;

            ost << std::setw(6) << i << " "
                << std::setw(sizeof(Tag) * 2) << std::setfill('0')
                << std::hex << std::uppercase << tag << ": "
                << std::setw(4) << std::setfill(' ') << std::dec << payload.refs << " \'";

            for (auto & c : payload.cells) {
                ost << c.seq;
            }

            ost << "\'" << std::endl;

            ++i;
        }

        ost << "END GLOBAL TAGS" << std::endl << std::endl;
    }

private:
    static Tag makeTag(const std::vector<Cell> & cells) {
        auto tag = hash<SDBM<Tag>>(&cells.front(), sizeof(Cell) * cells.size());
        if (tag == invalidTag()) { ++tag; }
        return tag;
    }
};

#endif // COMMON__DEDUPER__HXX
