// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/simple_deduper.hxx"
#include "terminol/support/hash.hxx"
#include "terminol/support/rle.hxx"

#include <algorithm>
#include <iostream>
#include <iomanip>

namespace {

void encode(const std::vector<Cell> & cells,
            std::vector<uint8_t>    & bytes) {
    OutMemoryStream os(bytes);

    // RLE encode the styles.
    std::vector<Style> styles;
    for (auto & cell : cells) {
        styles.push_back(cell.style);
    }
    rleEncode(styles, os);

    // Pack the string.
    std::vector<uint8_t> string;
    for (auto & cell : cells) {
        auto length = utf8::leadLength(cell.seq.lead());
        for (uint8_t i = 0; i != length; ++i) {
            string.push_back(cell.seq.bytes[i]);
        }
    }
    os.writeAll(&string.front(), 1, string.size());

#if 0
    auto before = cells.size() * sizeof(Cell);
    auto after  = bytes.size();

    std::cout << before << " --> " << after;
    if (before > 0) {
        std::cout << " " << double(after) / double(before) * 100.0;
    }
    std::cout << std::endl;
#endif
}

void decode(const std::vector<uint8_t> & bytes, std::vector<Cell> & cells) {
    InMemoryStream is(bytes);

    // RLE decode the styles
    std::vector<Style> styles;
    rleDecode(is, styles);

    for (auto & style : styles) {
        uint8_t b[4] = { 0 };

        is.readAll(&b[0], 1, 1);

        auto length = utf8::leadLength(b[0]);

        for (uint8_t i = 1; i != length; ++i) {
            is.readAll(&b[i], 1, 1);
        }

        utf8::Seq seq(b[0], b[1], b[2], b[3]);
        cells.push_back(Cell::utf8(seq, style));
    }
}

} // namespace {anonymous}

SimpleDeduper::SimpleDeduper() : _entries(), _totalRefs(0), _mutex() {}

auto SimpleDeduper::store(const std::vector<Cell> & cells) -> Tag {
    std::unique_lock<std::mutex> lock(_mutex);

    std::vector<uint8_t> bytes;
    encode(cells, bytes);
    auto tag = makeTag(bytes);

again:
    ASSERT(tag != invalidTag(), "");
    auto iter = _entries.find(tag);

    if (iter == _entries.end()) {
        _entries.insert(std::make_pair(tag, Entry(cells.size(), std::move(bytes))));
    }
    else {
        auto & entry = iter->second;

        if (bytes != entry.bytes) {
            std::cerr << "Hash collision: " << tag << std::endl;

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
    std::unique_lock<std::mutex> lock(_mutex);

    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");

    auto & bytes = iter->second.bytes;
    decode(bytes, cells);
}

void SimpleDeduper::lookupSegment(Tag tag, uint32_t offset, int16_t max_size,
                                  std::vector<Cell> & cells, bool & cont, int16_t & wrap) const {
    std::unique_lock<std::mutex> lock(_mutex);

    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");

    auto & bytes = iter->second.bytes;
    std::vector<Cell> tmp_cells;
    decode(bytes, tmp_cells);

    cells.resize(max_size, Cell::blank());
    wrap = std::min<uint32_t>(max_size, tmp_cells.size() - offset);

    std::copy(tmp_cells.begin() + offset, tmp_cells.begin() + offset + wrap, cells.begin());
    std::fill(cells.begin() + wrap, cells.end(), Cell::blank());
    cont = (offset + wrap != tmp_cells.size());
}

size_t SimpleDeduper::lookupLength(Tag tag) const {
    std::unique_lock<std::mutex> lock(_mutex);

    auto iter = _entries.find(tag);
    ASSERT(iter != _entries.end(), "");
    return iter->second.length;
}

void SimpleDeduper::remove(Tag tag) {
    std::unique_lock<std::mutex> lock(_mutex);

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
    std::unique_lock<std::mutex> lock(_mutex);

    uniqueLines = _entries.size();
    totalLines  = _totalRefs;
}

void SimpleDeduper::getByteStats(size_t & uniqueBytes, size_t & totalBytes) const {
    std::unique_lock<std::mutex> lock(_mutex);

    uniqueBytes = 0;
    totalBytes = 0;

    for (auto & l : _entries) {
        auto & entry = l.second;

        size_t size = entry.bytes.size();

        uniqueBytes += size;
        totalBytes  += entry.refs * size;
    }
}

void SimpleDeduper::dump(std::ostream & UNUSED(ost)) const {
    std::unique_lock<std::mutex> lock(_mutex);

#if 0
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
#endif
}

auto SimpleDeduper::makeTag(const std::vector<uint8_t> & bytes) -> Tag {
    auto tag = hash<SDBM<Tag>>(&bytes.front(), bytes.size());
    if (tag == invalidTag()) { ++tag; }
    return tag;
}
