// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/para_cache.hxx"

void ParaCache::shrink() {
    if (_maxEntries == 0) {
        return;
    }

    // Note, _mutex must be locked by this thread.

    auto iter = _cache.begin();

    while (_cache.size() > _maxEntries) {
        auto & entry = iter->second;
        if (entry.checkoutCount == 0) {
            iter = _cache.erase(iter);
        }
        else {
            break;
        }
    }
}

ParaCache::ParaCache(I_Repository & repository) :
    _repository(repository), _maxEntries(0) {}

ParaCache::~ParaCache() {
#if DEBUG
    for (auto & pair : _cache) {
        auto & entry = pair.second;
        ASSERT(entry.checkoutCount == 0,
               "Entry still checked out at destruction.");
    }
#endif
}

const Para & ParaCache::checkout(I_Repository::Tag tag) {
    std::unique_lock<std::mutex> lock(_mutex);

    auto iter = _cache.find(tag);

    if (iter == _cache.end()) {
        auto entry = _repository.retrieve(tag);
        iter  = _cache.insert(tag, Entry(Para(entry.styles, entry.string)));
        shrink();
    }

    auto & entry = iter->second;

    ++entry.checkoutCount;

    return entry.para;
}

void ParaCache::checkin(I_Repository::Tag tag) {
    std::unique_lock<std::mutex> lock(_mutex);

    auto iter = _cache.find(tag);

    ASSERT(iter != _cache.end(), "Tag not in cache.");

    auto & entry = iter->second;

    ASSERT(entry.checkoutCount > 0, "Reference count underflow.");

    --entry.checkoutCount;
}

void ParaCache::adjustMaxEntries(ssize_t delta) {
    std::unique_lock<std::mutex> lock(_mutex);

    if (delta < 0) {
        ASSERT(_maxEntries >= static_cast<size_t>(-delta), "maxEntries underflow.");
    }

    _maxEntries += delta;
    shrink();
}
