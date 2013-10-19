// vi:noai:sw=4

#ifndef COMMON__CLIENT__HXX
#define COMMON__CLIENT__HXX

#include "terminol/support/net.hxx"
#include "terminol/common/config.hxx"

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


#endif // COMMON__CLIENT__HXX
