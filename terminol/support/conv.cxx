// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/conv.hxx"

std::optional<std::vector<std::string>> split(const std::string & line,
                                              const std::string & delim) /*throw (ParseError)*/{
    auto i = line.find_first_not_of(delim);

    if (i == std::string::npos) { return std::nullopt; }   // blank line
    if (line[i] == '#')         { return std::nullopt; }   // comment

    std::vector<std::string> tokens;

    for (;;) {
        bool quote = line[i] == '"';

        auto j = line.find_first_of(quote ? "\"" : delim, i + 1);
        if (quote) { ++i; }

        if (j == std::string::npos) {
            if (quote) {
                throw ParseError("Unterminated quote.");
            }
            else {
                j = line.size();
            }
        }

        tokens.push_back(line.substr(i, j - i));

        if (j == line.size()) { break; }

        i = line.find_first_not_of(delim, j + 1);

        if (i == std::string::npos) { break; }
    }

    return tokens;
}
