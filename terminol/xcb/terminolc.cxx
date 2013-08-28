// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/cmdline.hxx"

#include <unistd.h>
#include <fcntl.h>

namespace {

std::string makeHelp(const std::string & progName) {
    std::ostringstream ost;
    ost << "terminolc " << VERSION << std::endl
        << "Usage: " << progName << " [OPTION]..." << std::endl
        << std::endl
        << "Options:" << std::endl
        << "  --help" << std::endl
        << "  --socket=SOCKET" << std::endl
        ;
    return ost.str();
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;
    parseConfig(config);

    CmdLine cmdLine(makeHelp(argv[0]), VERSION);
    cmdLine.add(new StringHandler(config.socketPath), '\0', "socket");

    // Command line

    try {
        cmdLine.parse(argc, const_cast<const char **>(argv));
    }
    catch (const CmdLine::Error & ex) {
        FATAL(ex.message);
    }

    const auto & socketPath = config.socketPath;
    auto fd = ::open(socketPath.c_str(), O_WRONLY | O_NONBLOCK);

    if (fd == -1) {
        std::cerr << "Failed to open: " << socketPath << std::endl;
        return 1;
    }

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
