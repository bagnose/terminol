// vi:noai:sw=4

#ifndef COMMON__PARSER__HXX
#define COMMON__PARSER__HXX

#include <string>
#include <vector>

#include "terminol/common/config.hxx"

bool split(const std::string & line,
           std::vector<std::string> & tokens,
           const std::string & delim = " \t") throw (ParseError);

void parseConfig(Config & config);

#endif // COMMON__PARSER__HXX
