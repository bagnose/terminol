// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__PIPE__HXX
#define SUPPORT__PIPE__HXX

#include "terminol/support/debug.hxx"

class Pipe {
public:
    Pipe();
    ~Pipe();

    int readFd()  { return _fds[0]; }
    int writeFd() { return _fds[1]; }

private:
    int _fds[2];
};

#endif // SUPPORT__PIPE__HXX
