// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__PIPE__HXX
#define SUPPORT__PIPE__HXX

#include "terminol/support/sys.hxx"
#include "terminol/support/debug.hxx"

#include <fcntl.h>
#include <unistd.h>

class Pipe {
    int _fds[2];

public:
    Pipe() noexcept {
#ifdef __linux__
        // pipe2() doesn't raise EINTR.
        ENFORCE_SYS(::pipe2(_fds, O_NONBLOCK | O_CLOEXEC) != -1, "");
#else
        // pipe() doesn't raise EINTR.
        ENFORCE_SYS(::pipe(_fds) != -1, "");
        fdCloseExec(_fds[0]);
        fdCloseExec(_fds[1]);
        fdNonBlock(_fds[0]);
        fdNonBlock(_fds[1]);
#endif
    }

    ~Pipe() noexcept {
        ENFORCE_SYS(::close(_fds[0]) != -1, "");
        ENFORCE_SYS(::close(_fds[1]) != -1, "");
    }

    int readFd() noexcept { return _fds[0]; }
    int writeFd() noexcept { return _fds[1]; }
};

#endif // SUPPORT__PIPE__HXX
