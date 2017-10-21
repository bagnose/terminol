// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__SIMPLE_REPOSITORY__HXX
#define COMMON__SIMPLE_REPOSITORY__HXX

#include "terminol/common/repository_interface.hxx"

#include <mutex>
#include <unordered_map>

class SimpleRepository final : public I_Repository {
    mutable std::mutex             _mutex;
    Tag                            _nextTag = 0;
    std::unordered_map<Tag, Entry> _entries;

public:
    SimpleRepository();

    Tag      store(const Entry & entry) override;
    Entry    retrieve(Tag tag) const override;
    uint32_t length(Tag tag) const override;
    bool     match(Tag tag, const std::vector<Regex> & regexes) const override;
    void     discard(Tag tag) override;
    void     dump(std::ostream & ost) const override;
};

#endif // COMMON__SIMPLE_REPOSITORY__HXX
