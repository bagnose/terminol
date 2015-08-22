// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/test.hxx"

#include <iostream>

#include <unistd.h>

bool Test::common(bool success, const std::string & message) {
    ++_count;
    if (success) {
        ++_successes;
    }
    else {
        ++_failures;
    }

    const char * pass;
    const char * fail;

    if (isatty(STDERR_FILENO)) {
        pass = "\033[0;32m" "PASS" "\033[0m";
        fail = "\033[0;31m" "FAIL" "\033[0m";
    }
    else {
        pass = "PASS";
        fail = "FAIL";
    }

    std::ostringstream sst;
    sst << (success ? pass : fail)
        << " " << _count
        << " " << getPath() << " -"
        << " " << message;
    std::cerr << sst.str() << std::endl;

    return success;
}
