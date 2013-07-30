// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/support/debug.hxx"

#include <unistd.h>
#include <fcntl.h>

namespace {

bool argMatch(const std::string & arg, const std::string & opt, std::string & val) {
    std::string optComposed = "--" + opt + "=";
    if (arg.substr(0, optComposed.size()) ==  optComposed) {
        val = arg.substr(optComposed.size());
        return true;
    }
    else {
        return false;
    }
}

void showHelp(const std::string & progName, std::ostream & ost) {
    ost << "terminolc " << VERSION << std::endl
        << "Usage: " << progName << " [OPTION]..." << std::endl
        << std::endl
        << "Options:" << std::endl
        << "  --help" << std::endl
        << "  --socket=SOCKET" << std::endl
        ;
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;
    parseConfig(config);

    // Command line

    try {
        for (int i = 1; i != argc; ++i) {
            std::string arg = argv[i];
            std::string val;

            if (arg == "--help") {
                showHelp(argv[0], std::cout);
                return 0;
            }
            else if (argMatch(arg, "socket", val)) {
                config.socketPath = val;
            }
            else {
                std::cerr << "Unrecognised argument '" << arg << "'" << std::endl;
                showHelp(argv[0], std::cerr);
                return 2;
            }
        }
    }
    catch (const ParseError & ex) {
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
