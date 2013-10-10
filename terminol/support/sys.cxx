// vi:noai:sw=4

#include "terminol/support/sys.hxx"
#include "terminol/support/debug.hxx"

#include <unistd.h>
#include <fcntl.h>

// Set FD_CLOEXEC
void fdCloseExec(int fd) {
    ENFORCE_SYS(fd != -1, "");
    int flags;
    ENFORCE_SYS((flags = TEMP_FAILURE_RETRY(::fcntl(fd, F_GETFD))) != -1, "");
    flags |= FD_CLOEXEC;
    ENFORCE_SYS(TEMP_FAILURE_RETRY(::fcntl(fd, F_SETFD, flags)) != -1, "");
}

// Set O_NONBLOCK
void fdNonBlock(int fd) {
    ENFORCE_SYS(fd != -1, "");
    int flags;
    ENFORCE_SYS((flags = TEMP_FAILURE_RETRY(::fcntl(fd, F_GETFL))) != -1, "");
    flags |= O_NONBLOCK;
    ENFORCE_SYS(TEMP_FAILURE_RETRY(::fcntl(fd, F_SETFL, flags)) != -1, "");
}
