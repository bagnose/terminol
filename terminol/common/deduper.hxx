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
            const auto & line = payload.line;

            if (cont  != line.cont ||
                wrap  != line.wrap ||
                cells != line.cells) {
#if 0
#if DEBUG
                std::cerr << "Collision between:" << std::endl;

                {
                    ASSERT(wrap <= cells.size(), wrap << " < " << cells.size());
                    std::cerr << cont << " " << std::setw(3) << wrap << " \'";
                    std::cerr << "   '";
                    uint16_t col = 0;
                    std::cerr << SGR::UNDERLINE;
                    for (; col != wrap; ++col) { std::cerr << cells[col].seq; }
                    std::cerr << SGR::RESET_UNDERLINE;
                    for (; col != cells.size(); ++col) { std::cerr << cells[col].seq; }
                    std::cerr << "\'" << std::endl;
                }

                std::cerr << "And:" << std::endl;

                {
                    ASSERT(line.wrap <= line.cells.size(), line.wrap << " < " << line.cells.size());
                    std::cerr << line.cont << " " << std::setw(3) << line.wrap << " \'";
                    std::cerr << "   '";
                    uint16_t col = 0;
                    std::cerr << SGR::UNDERLINE;
                    for (; col != line.wrap; ++col) { std::cerr << line.cells[col].seq; }
                    std::cerr << SGR::RESET_UNDERLINE;
                    for (; col != line.cells.size(); ++col) { std::cerr << line.cells[col].seq; }
                }
                std::cerr << "\'" << std::endl;
#endif
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

    void getStats(uint32_t & uniqueLines, uint32_t & totalLines) const {
        uniqueLines = _lines.size();
        totalLines  = _totalRefCount;
    }

    double getReduction() const {
        if (_lines.empty()) {
            return 1.0;
        }
        else {
            return
                static_cast<double>(_totalRefCount) /
                static_cast<double>(_lines.size());
        }
    }

private:
    Tag makeTag(const std::vector<Cell> & cells) const {
        return hash<SDBM<Tag>>(&cells.front(), sizeof(Cell) * cells.size());
    }
};

#endif // COMMON__DEDUPER__HXX
