// vi:noai:sw=4
// Copyright © 2017 David Bryant

#ifndef COMMON__PARA_CACHE__HXX
#define COMMON__PARA_CACHE__HXX

#include "terminol/common/repository_interface.hxx"
#include "terminol/common/para.hxx"
#include "terminol/support/cache.hxx"
#include "terminol/support/pattern.hxx"

#include <unordered_map>

class ParaCache : private Uncopyable {
    I_Repository                   & _repository;
    Cache<I_Repository::Tag, Para>   _cache;
    size_t                           _maxEntries;

public:
    ParaCache(I_Repository & repository, size_t maxEntries = 0);
    ParaCache(ParaCache &&) = default;

    const Para & get(I_Repository::Tag tag);
    void setMaxEntries(size_t maxEntries);

private:
    void shrink();
};

#endif // COMMON__PARA_CACHE__HXX
