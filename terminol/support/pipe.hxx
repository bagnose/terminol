// vi:noai:sw=4

#ifndef SUPPORT__PIPE__HXX
#define SUPPORT__PIPE__HXX

#include <fcntl.h>
#include <unistd.h>

class Pipe {
    int _fds[2];

public:
    Pipe() {
        ENFORCE_SYS(TEMP_FAILURE_RETRY(::pipe2(_fds, O_NONBLOCK | O_CLOEXEC)) != -1, "");
    }

    ~Pipe() {
        ENFORCE_SYS(::close(_fds[0]) != -1, "");
        ENFORCE_SYS(::close(_fds[1]) != -1, "");
    }

    int readFd() { return _fds[0]; }
    int writeFd() { return _fds[1]; }
};

#endif // SUPPORT__PIPE__HXX
