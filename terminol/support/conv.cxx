// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/conv.hxx"

bool split(const std::string        & line,
           std::vector<std::string> & tokens,
           const std::string        & delim) /*throw (ParseError)*/ {
    auto i = line.find_first_not_of(delim);

    if (i == std::string::npos) { return false; }   // blank line
    if (line[i] == '#')         { return false; }   // comment

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

    return true;
}
