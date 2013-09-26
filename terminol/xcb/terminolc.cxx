// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/common/parser.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/cmdline.hxx"
#include "terminol/support/net.hxx"

class Client : protected SocketClient::I_Observer {
    SocketClient   _socket;
    bool         & _done;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Client(I_Selector   & selector,
           const Config & config,
           bool           shutdown,
           bool         & done) try :
        _socket(*this, selector, config.socketPath),
        _done(done)
    {
        uint8_t byte = shutdown ? 0xFF : 0;
        _socket.send(&byte, 1);
    }
    catch (const SocketClient::Error & ex) {
        throw Error(ex.message);
    }

    virtual ~Client() {}

protected:

    // SocketClient::I_Observer implementation:

    void clientDisconnected() throw () {
        ERROR("Client disconnected");
        _done = true;
    }

    void clientQueueEmpty() throw () {
        _done = true;
    }
};

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
    parseConfig(config);

    bool shutdown = false;

    CmdLine cmdLine(makeHelp(argv[0]), VERSION);
    cmdLine.add(new StringHandler(config.socketPath), '\0', "socket");
    cmdLine.add(new BoolHandler(shutdown), '\0', "shutdown");

    // Command line

    try {
        cmdLine.parse(argc, const_cast<const char **>(argv));
    }
    catch (const CmdLine::Error & ex) {
        FATAL(ex.message);
    }

    Selector selector;

    try {
        bool done = false;
        Client client(selector, config, shutdown, done);

        while (!done) {
            selector.animate();
        }
    }
    catch (const Client::Error & ex) {
        FATAL(ex.message);
    }

    return 0;
}
