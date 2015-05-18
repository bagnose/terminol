// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "dedupe_repository.hxx"
#include "terminol/support/rle.hxx"
#include "terminol/support/hash.hxx"

std::vector<uint8_t> DedupeRepository::encode(const Entry & entry) {
    std::vector<uint8_t> bytes;
    OutMemoryStream os(bytes, true);

    // Write the size of the string followed by the string content.
    uint32_t size = entry.string.size();
    os.writeAll(&size, sizeof size, 1);
    os.writeAll(&entry.string.front(), 1, entry.string.size());

    // RLE encode the styles
    rleEncode(entry.styles, os);

    return bytes;
}

auto DedupeRepository::decode(const std::vector<uint8_t> & bytes) -> Entry {
    Entry entry;
    InMemoryStream is(bytes);

    // Read the size of the string followed by the string content.
    uint32_t size;
    is.readAll(&size, sizeof size, 1);
    entry.string.resize(size);
    is.readAll(&entry.string.front(), 1, entry.string.size());

    // RLE decode the styles
    rleDecode(is, entry.styles);

    return entry;
}

auto DedupeRepository::makeTag(const std::vector<uint8_t> & bytes) -> Tag {
    return hash<SDBM<Tag>>(&bytes.front(), bytes.size());
}

DedupeRepository::DedupeRepository() : _totalRefs(0) {}

DedupeRepository::~DedupeRepository() {}

auto DedupeRepository::store(const Entry & entry) -> Tag {
    auto bytes = encode(entry);

    std::unique_lock<std::mutex> lock(_mutex);

    auto tag = makeTag(bytes);

again:
    auto iter = _entries.find(tag);

    if (iter == _entries.end()) {
        _entries.insert(std::make_pair(tag, DedupeEntry(entry.styles.size(), bytes)));
    }
    else {
        auto & dedupeEntry = iter->second;

        if (bytes != dedupeEntry.bytes) {
            // Hash collision.
            ENFORCE(_entries.size() < std::numeric_limits<Tag>::max(),
                    "No dedupe room left.");

            ++tag;
            goto again;
        }
        else if (dedupeEntry.refs == std::numeric_limits<uint32_t>::max()) {
            // Avoid ref count overflow.
            ENFORCE(_entries.size() < std::numeric_limits<Tag>::max(),
                    "No dedupe room left.");

            ++tag;
            goto again;
        }
        else {
            ++dedupeEntry.refs;
        }
    }

    ++_totalRefs;

    return tag;
}

auto DedupeRepository::retrieve(Tag tag) const -> Entry {
    std::unique_lock<std::mutex> lock(_mutex);

    auto & dedupeEntry = _entries.at(tag);
    auto entry = decode(dedupeEntry.bytes);
    ASSERT(entry.styles.size() == dedupeEntry.length, "");
    return entry;
}

uint32_t DedupeRepository::length(Tag tag) const {
    std::unique_lock<std::mutex> lock(_mutex);

    auto & entry =_entries.at(tag);
    return entry.length;
}

bool DedupeRepository::match(Tag tag, const std::vector<Regex> & regexes) const {
    std::unique_lock<std::mutex> lock(_mutex);

    auto & entry = _entries.at(tag);
    uint32_t size;

    {
        InMemoryStream is(entry.bytes);
        is.readAll(&size, sizeof size, 1);
    }

    auto str  = &entry.bytes.front() + sizeof size;
    auto str2 = reinterpret_cast<const char *>(str);

    for (auto & regex : regexes) {
        if (regex.matchTest(str2, size)) {
            return true;
        }
    }

    return false;
}

void DedupeRepository::discard(Tag tag) {
    std::unique_lock<std::mutex> lock(_mutex);

    auto iter = _entries.find(tag);

    if (iter == _entries.end()) {
        throw std::out_of_range("Couldn't find tag.");
    }

    auto & entry = iter->second;

    if (--entry.refs == 0) {
        _entries.erase(iter);
    }

    --_totalRefs;
}
