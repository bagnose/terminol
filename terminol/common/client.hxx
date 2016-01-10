// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__CLIENT__HXX
#define COMMON__CLIENT__HXX

#include "terminol/support/net.hxx"
#include "terminol/common/config.hxx"

class Client : protected SocketClient::I_Observer {
    SocketClient   _socket;
    bool           _finished;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Client(I_Selector   & selector,
           const Config & config,
           bool           shutdown) try :
        _socket(*this, selector, config.socketPath),
        _finished(false)
    {
        uint8_t byte = shutdown ? 0xFF : 0;
        _socket.send(&byte, 1);
    }
    catch (const SocketClient::Error & error) {
        throw Error(error.message);
    }

    virtual ~Client() {}

    bool isFinished() const { return _finished; }

protected:

    // SocketClient::I_Observer implementation:

    void clientDisconnected() override {
        ERROR("Client disconnected");
        _finished = true;
    }

    void clientQueueEmpty() override {
        _finished = true;
    }
};

#endif // COMMON__CLIENT__HXX
