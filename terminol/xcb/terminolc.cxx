// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/support/debug.hxx"

#include <unistd.h>
#include <fcntl.h>

int main() {
    Config config;

    const std::string & socketPath = config.getSocketPath();
    int fd = ::open(socketPath.c_str(), O_WRONLY | O_NONBLOCK);

    ENFORCE_SYS(fd != -1, "Failed to open: " << socketPath);

    char c;
    ENFORCE_SYS(::write(fd, &c, 1) != -1, "");

    ENFORCE_SYS(::close(fd) != -1, "");

    return 0;
}
