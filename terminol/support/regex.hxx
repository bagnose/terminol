// vi:noai:sw=4

#ifndef SUPPORT__REGEX__HXX
#define SUPPORT__REGEX__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

#include <vector>
#include <string>

#include <pcre.h>

// Regex r("foo");
// r.match("foo bar");

class Regex : protected Uncopyable {
    pcre * _pcre;
public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Regex(const std::string & pattern) throw (Error) {
        const char * err;
        int          errOffset;

        _pcre = pcre_compile(pattern.c_str(),
                             PCRE_UTF8,
                             &err,
                             &errOffset,
                             NULL);     // tableptr

        if (_pcre == NULL) {
            throw Error("PCRE commpilation of \"" + pattern + "\" "
                        "failed at offset " + stringify(errOffset) +
                        ", error: " + stringify(err));
        }
    }

    ~Regex() {
        pcre_free(_pcre);
    }

    // First element is "whole match", subsequent are "captures" (things in parentheses).
    std::vector<std::string> match(const std::string & text) {
        const int offsetsSize = 3 * 10;        // Multiple of 3
        int offsets[offsetsSize];


        auto rval = pcre_exec(_pcre,
                              NULL,             // study
                              text.data(),      // subject
                              text.size(),      // length of subject
                              0,                // offset into subject
                              0,                // options
                              offsets,
                              offsetsSize);

        std::vector<std::string> matches;

        if (rval < 0) {
            switch (rval) {
                case PCRE_ERROR_NOMATCH:
                    break;
                default:
                    ERROR("Unknown error matching.");
                    break;
            }
        }
        else if (rval == 0) {
            ERROR("Insufficient match room.");
        }
        else {
            for (auto i = 0; i != rval; ++i) {
                auto a = offsets[2 * i];
                auto b = offsets[2 * i + 1];

                const char * start = text.data() + a;
                const char * end   = text.data() + b;
                matches.push_back(std::string(start, end));
            }
        }

        return matches;
    }
};

#endif // SUPPORT__REGEX__HXX
