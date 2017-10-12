// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__DEDUPE_REPOSITORY__HXX
#define COMMON__DEDUPE_REPOSITORY__HXX

#include "terminol/common/repository_interface.hxx"

#include <mutex>
#include <unordered_map>

class DedupeRepository final : public I_Repository {
    struct DedupeEntry {
        uint32_t             length;
        std::vector<uint8_t> bytes;
        uint32_t             refs = 1;
    };

    mutable std::mutex                   _mutex;
    std::unordered_map<Tag, DedupeEntry> _entries;
    size_t                               _totalRefs;

public:
    DedupeRepository();

    Tag      store(const Entry & entry) override;
    Entry    retrieve(Tag tag) const override;
    uint32_t length(Tag tag) const override;
    bool     match(Tag tag, const std::vector<Regex> & regexes) const override;
    void     discard(Tag tag) override;
    void     dump(std::ostream & ost) const override;

private:
    static std::vector<uint8_t> encode(const Entry & entry);
    static Entry decode(const std::vector<uint8_t> & bytes);
    static Tag makeTag(const std::vector<uint8_t> & bytes);
};

#endif // COMMON__DEDUPE_REPOSITORY__HXX
