// vi:noai:sw=4
// Copyright © 2016 David Bryant

#include "terminol/common/para_cache.hxx"

ParaCache::ParaCache(I_Repository & repository, size_t maxEntries)
    : _repository(repository), _maxEntries(maxEntries) {}

const Para & ParaCache::get(I_Repository::Tag tag) {
    auto iter = _cache.find(tag);

    if (iter == _cache.end()) {
        auto entry = _repository.retrieve(tag);
        iter       = _cache.insert(tag, Para(entry.styles, entry.string));
        shrink();
    }

    return iter->second;
}

void ParaCache::setMaxEntries(size_t maxEntries) {
    _maxEntries = maxEntries;
    shrink();
}

void ParaCache::shrink() {
    if (_maxEntries == 0) { return; }

    while (_cache.size() > _maxEntries) { _cache.erase(_cache.begin()); }
}
