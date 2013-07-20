// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/support/debug.hxx"

#include <unistd.h>
#include <fcntl.h>

int main() {
    Config config;
    parseConfig(config);

    const auto & socketPath = config.socketPath;
    auto fd = ::open(socketPath.c_str(), O_WRONLY | O_NONBLOCK);

    ENFORCE_SYS(fd != -1, "Failed to open: " << socketPath);

    char c = 0;
    auto rval = TEMP_FAILURE_RETRY(::write(fd, &c, 1));

    if (rval == -1) {
        switch (errno) {
            case EAGAIN:
                // The server does not have the socket open for
                // read. Note, this can be caused by a race condition
                // due to the el-cheapo FIFO scheme.
                FATAL("Would block");
                break;
            default:
                FATAL("");
                break;
        }
    }

    ENFORCE_SYS(::close(fd) != -1, "");

    return 0;
}
