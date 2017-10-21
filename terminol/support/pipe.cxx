// Copyright © 2017 David Bryant

#include "terminol/support/pipe.hxx"
#include "terminol/support/sys.hxx"
#include "terminol/support/exception.hxx"

#include <system_error>

#include <fcntl.h>
#include <unistd.h>

Pipe::Pipe() {
#ifdef __linux__
    // pipe2() doesn't raise EINTR.
    THROW_IF_SYSCALL_FAILS(::pipe2(_fds, O_NONBLOCK | O_CLOEXEC), "pipe2()");
#else
    // pipe() doesn't raise EINTR.
    THROW_IF_SYSCALL_FAILS(::pipe(_fds), "pipe()");
    fdCloseExec(_fds[0]);
    fdCloseExec(_fds[1]);
    fdNonBlock(_fds[0]);
    fdNonBlock(_fds[1]);
#endif
}

Pipe::~Pipe() {
    TEMP_FAILURE_RETRY(::close(_fds[0]));
    TEMP_FAILURE_RETRY(::close(_fds[1]));
}
