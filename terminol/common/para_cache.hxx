// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__PARA_CACHE__HXX
#define COMMON__PARA_CACHE__HXX

#include "terminol/common/repository_interface.hxx"
#include "terminol/common/para.hxx"
#include "terminol/support/cache.hxx"
#include "terminol/support/pattern.hxx"

#include <mutex>
#include <unordered_map>

class ParaCache : private Uncopyable {
    struct Entry {
        Entry(const Para & para_) : para(para_), checkoutCount(0) {}

        Para     para;
        uint32_t checkoutCount;
    };

    std::mutex                       _mutex;
    I_Repository                   & _repository;
    Cache<I_Repository::Tag, Entry>  _cache;
    size_t                           _maxEntries;

    void shrink();

public:
    explicit ParaCache(I_Repository & repository);
    ~ParaCache();

    const Para & checkout(I_Repository::Tag tag);
    void checkin(I_Repository::Tag tag);
    void adjustMaxEntries(ssize_t delta);
};

#endif // COMMON__PARA_CACHE__HXX
