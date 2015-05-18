// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__DEDUPE_REPOSITORY__HXX
#define COMMON__DEDUPE_REPOSITORY__HXX

#include "terminol/common/repository_interface.hxx"

#include <mutex>
#include <unordered_map>

class DedupeRepository : public I_Repository {
    static std::vector<uint8_t> encode(const Entry & entry);
    static Entry decode(const std::vector<uint8_t> & bytes);
    static Tag makeTag(const std::vector<uint8_t> & bytes);

    struct DedupeEntry {
        explicit DedupeEntry(int32_t length_, const std::vector<uint8_t> & bytes_) :
            refs(1), length(length_), bytes(bytes_) {}

        uint32_t             refs;
        uint32_t             length;
        std::vector<uint8_t> bytes;
    };

    mutable std::mutex                   _mutex;
    std::unordered_map<Tag, DedupeEntry> _entries;
    size_t                               _totalRefs;

public:
    DedupeRepository();
    virtual ~DedupeRepository();

    Tag      store(const Entry & entry) override;
    Entry    retrieve(Tag tag) const override;
    uint32_t length(Tag tag) const override;
    bool     match(Tag tag, const std::vector<Regex> & regexes) const override;
    void     discard(Tag tag) override;
};

#endif // COMMON__DEDUPE_REPOSITORY__HXX
