// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/sys.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/exception.hxx"

#include <unistd.h>
#include <fcntl.h>

// Set FD_CLOEXEC
void fdCloseExec(int fd) {
    ASSERT(fd != -1, );
    int flags = THROW_IF_SYSCALL_FAILS(::fcntl(fd, F_GETFD), "fcntl()");
    flags |= FD_CLOEXEC;
    THROW_IF_SYSCALL_FAILS(::fcntl(fd, F_SETFD, flags), "fcntl()");
}

// Set O_NONBLOCK
void fdNonBlock(int fd) {
    ASSERT(fd != -1, );
    int flags;
    THROW_IF_SYSCALL_FAILS((flags = ::fcntl(fd, F_GETFL)), "fcntl()");
    flags |= O_NONBLOCK;
    THROW_IF_SYSCALL_FAILS(::fcntl(fd, F_SETFL, flags), "fcntl()");
}
