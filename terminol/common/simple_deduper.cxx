// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/simple_deduper.hxx"

#include <algorithm>
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

SimpleDeduper::SimpleDeduper() : _entries(), _totalRefs(0) {}

SimpleDeduper::~SimpleDeduper() {}

auto SimpleDeduper::store(const std::vector<Cell> & cells) -> Tag {
    auto tag = makeTag(cells);

again:
    ASSERT(tag != invalidTag(), "");
    auto iter = _entries.find(tag);

    if (iter == _entries.end()) {
        _entries.insert(std::make_pair(tag, Entry(cells)));
    }
    else {
        auto & entry = iter->second;

        if (cells != entry.cells) {
#if 0
            std::cerr << "Hash collision:" << std::endl;

            std::cerr << "  \'";
            for (auto & c : cells) {
                std::cerr << c.seq;
            }
            std::cerr << "\'" << std::endl;

            std::cerr << "  \'";
            for (auto & c : entry.cells) {
                std::cerr << c.seq;
            }
            std::cerr << "\'" << std::endl;
#endif

            ENFORCE(static_cast<Tag>(_entries.size()) != invalidTag(), "No dedupe room left.");

            ++tag;
            if (tag == invalidTag()) { ++tag; }
            goto again;
        }

        ++entry.refs;
    }

    ++_totalRefs;

    return tag;
}

void SimpleDeduper::lookup(Tag tag, std::vector<Cell> & cells) const {
    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");

    auto & c = iter->second.cells;
    cells.resize(c.size(), Cell::blank());
    std::copy(c.begin(), c.end(), cells.begin());
}

void SimpleDeduper::lookupSegment(Tag tag, uint32_t offset, int16_t max_size,
                                  std::vector<Cell> & cells, bool & cont, int16_t & wrap) const {
    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");

    auto & c = iter->second.cells;
    cells.resize(max_size, Cell::blank());
    wrap = std::min<uint32_t>(max_size, c.size() - offset);

    std::copy(c.begin() + offset, c.begin() + offset + wrap, cells.begin());
    std::fill(cells.begin() + wrap, cells.end(), Cell::blank());
    cont = (offset + wrap != c.size());
}

size_t SimpleDeduper::lookupLength(Tag tag) const {
    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");

    auto & c = iter->second.cells;
    return c.size();
}

void SimpleDeduper::remove(Tag tag) {
    ASSERT(tag != invalidTag(), "");
    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");
    auto & entry = iter->second;

    if (--entry.refs == 0) {
        _entries.erase(iter);
    }

    --_totalRefs;
}

void SimpleDeduper::getLineStats(uint32_t & uniqueLines, uint32_t & totalLines) const {
    uniqueLines = _entries.size();
    totalLines  = _totalRefs;
}

void SimpleDeduper::getByteStats(size_t & uniqueBytes, size_t & totalBytes) const {
    uniqueBytes = 0;
    totalBytes = 0;

    for (auto & l : _entries) {
        auto & payload = l.second;

        size_t size = payload.cells.size() * sizeof(Cell);

        uniqueBytes += size;
        totalBytes  += payload.refs * size;
    }
}

void SimpleDeduper::dump(std::ostream & ost) const {
    ost << "BEGIN GLOBAL TAGS" << std::endl;

    auto flags = ost.flags();

    size_t i = 0;

    for (auto & l : _entries) {
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

    ost.flags(flags);

    ost << "END GLOBAL TAGS" << std::endl << std::endl;
}

auto SimpleDeduper::makeTag(const std::vector<Cell> & cells) -> Tag {
    auto tag = hash<SDBM<Tag>>(&cells.front(), sizeof(Cell) * cells.size());
    if (tag == invalidTag()) { ++tag; }
    return tag;
}
