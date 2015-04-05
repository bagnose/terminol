// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/common/client.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/cmdline.hxx"

namespace {

std::string makeHelp(const std::string & progName) {
    std::ostringstream ost;
    ost << "terminolc " << VERSION << std::endl
        << "Usage: " << progName << " [OPTION]..." << std::endl
        << std::endl
        << "Options:" << std::endl
        << "  --help" << std::endl
        << "  --socket=SOCKET" << std::endl
        << "  --shutdown" << std::endl
        ;
    return ost.str();
}

} // namespace {anonymous}

int main(int argc, char * argv[]) {
    Config config;

    try {
        parseConfig(config);
    }
    catch (const ParseError & error) {
        FATAL(error.message);
    }

    bool shutdown = false;

    CmdLine cmdLine(makeHelp(argv[0]), VERSION);
    cmdLine.add(new StringHandler(config.socketPath), '\0', "socket");
    cmdLine.add(new BoolHandler(shutdown), '\0', "shutdown");

    // Command line

    try {
        cmdLine.parse(argc, const_cast<const char **>(argv));
    }
    catch (const CmdLine::Error & error) {
        FATAL(error.message);
    }

    Selector selector;

    try {
        Client client(selector, config, shutdown);

        do {
            selector.animate();
        } while (!client.isFinished());
    }
    catch (const Client::Error & error) {
        FATAL(error.message);
    }

    return 0;
}
