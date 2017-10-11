// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/simple_repository.hxx"

SimpleRepository::SimpleRepository() : _nextTag(0) {}

auto SimpleRepository::store(const Entry & entry) -> Tag {
    std::unique_lock<std::mutex> lock(_mutex);

    for (;;) {
        auto pair = _entries.insert(std::make_pair(_nextTag, entry));

        if (pair.second) {
            return _nextTag++;
        }

        ++_nextTag;
    }
}

auto SimpleRepository::retrieve(Tag tag) const -> Entry {
    std::unique_lock<std::mutex> lock(_mutex);

    return _entries.at(tag);
}

uint32_t SimpleRepository::length(Tag tag) const {
    std::unique_lock<std::mutex> lock(_mutex);

    auto & entry = _entries.at(tag);

    return entry.styles.size();
}

bool SimpleRepository::match(Tag UNUSED(tag), const std::vector<Regex> & UNUSED(regexes)) const {
    return false;
}

void SimpleRepository::discard(Tag tag) {
    std::unique_lock<std::mutex> lock(_mutex);

    _entries.erase(tag);
}

void SimpleRepository::dump(std::ostream & ost) const {
    std::unique_lock<std::mutex> lock(_mutex);

    for (auto & pair : _entries) {
        auto   tag   = pair.first;
        auto & entry = pair.second;

        ost << tag << ": ";

        for (auto byte : entry.string) {
            ost << byte;
        }

        ost << std::endl;
    }
}
