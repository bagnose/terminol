// vi:noai:sw=4

#ifndef COMMON__SERVER__HXX
#define COMMON__SERVER__HXX

#include "terminol/common/config.hxx"
#include "terminol/support/net.hxx"

class I_Creator {
public:
    virtual void create() throw () = 0;
    virtual void shutdown() throw () = 0;

protected:
    I_Creator() {}
    ~I_Creator() {}
};

//
//
//

class Server : protected SocketServer::I_Observer {
    I_Creator    & _creator;
    SocketServer   _socket;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Server(I_Creator    & creator,
           I_Selector   & selector,
           const Config & config) throw (Error) try :
        _creator(creator),
        _socket(*this, selector, config.socketPath)
    {}
    catch (const SocketServer::Error & ex) {
        throw Error(ex.message);
    }

    virtual ~Server() {}

protected:

    // SocketServer::I_Observer implementation:

    void serverConnected(int UNUSED(id)) throw () {
        //PRINT("Server connected: " << id);
    }

    void serverReceived(int id, const uint8_t * data, size_t UNUSED(size)) throw () {
        //PRINT("Server received bytes, " << id << ": " << size << "b");

        if (data[0] == 0xFF) {
            _creator.shutdown();
        }
        else {
            _creator.create();
        }

        _socket.disconnect(id);
    }

    void serverDisconnected(int UNUSED(id)) throw () {
        //PRINT("Server disconnected: " << id);
    }
};

#endif // COMMON__SERVER__HXX
