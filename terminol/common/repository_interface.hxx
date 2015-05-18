// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__REPOSITORY_INTERFACE__HXX
#define COMMON__REPOSITORY_INTERFACE__HXX

#include "terminol/common/data_types.hxx"
#include "terminol/support/regex.hxx"

#include <vector>

class I_Repository {
public:
    typedef uint32_t Tag;       // Note, can use smaller tag sizes to cause collisions, etc.

    struct Entry {
        Entry() {}

        Entry(const std::vector<Style> & styles_, const std::vector<uint8_t> & string_) :
            styles(styles_), string(string_) {}

        std::vector<Style>   styles;
        std::vector<uint8_t> string;
    };

    virtual Tag      store(const Entry & entry) = 0;
    virtual Entry    retrieve(Tag tag) const = 0;
    virtual uint32_t length(Tag tag) const = 0;
    virtual bool     match(Tag tag, const std::vector<Regex> & regexes) const = 0;
    virtual void     discard(Tag tag) = 0;

protected:
    I_Repository()  = default;
    ~I_Repository() = default;
};

#endif // COMMON__REPOSITORY_INTERFACE__HXX
