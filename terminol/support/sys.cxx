// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/sys.hxx"
#include "terminol/support/debug.hxx"

#include <unistd.h>
#include <fcntl.h>

// Set FD_CLOEXEC
void fdCloseExec(int fd) noexcept {
    ENFORCE_SYS(fd != -1, "");
    int flags;
    ENFORCE_SYS((flags = ::fcntl(fd, F_GETFD)) != -1, "");      // No EINTR.
    flags |= FD_CLOEXEC;
    ENFORCE_SYS(::fcntl(fd, F_SETFD, flags) != -1, "");     // No EINTR.
}

// Set O_NONBLOCK
void fdNonBlock(int fd) noexcept {
    ENFORCE_SYS(fd != -1, "");
    int flags;
    ENFORCE_SYS((flags = ::fcntl(fd, F_GETFL)) != -1, "");      // No EINTR.
    flags |= O_NONBLOCK;
    ENFORCE_SYS(::fcntl(fd, F_SETFL, flags) != -1, "");     // No EINTR.
}
