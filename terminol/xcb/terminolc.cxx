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
            << "  --shutdown" << std::endl;
        return ost.str();
    }

} // namespace

int main(int argc, char * argv[]) try {
    Config config;

    parseConfig(config);

    bool shutdown = false;

    CmdLine cmdLine(makeHelp(argv[0]), VERSION);
    cmdLine.add(std::make_unique<StringHandler>(config.socketPath), '\0', "socket");
    cmdLine.add(std::make_unique<BoolHandler>(shutdown), '\0', "shutdown");

    // Command line

    cmdLine.parse(argc, const_cast<const char **>(argv));

    Selector selector;

    Client client(selector, config, shutdown);

    do { selector.animate(); } while (!client.isFinished());

    return 0;
}
catch (const UserError & ex) {
    std::cerr << ex.message() << std::endl;
    return 1;
}
