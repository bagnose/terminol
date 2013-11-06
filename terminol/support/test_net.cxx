// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/support/net.hxx"

class TestServer : protected SocketServer::I_Observer {
    SocketServer   _socket;
    size_t       & _count;

public:
    TestServer(I_Selector & selector, const std::string & path, size_t & count) :
        _socket(*this, selector, path), _count(count) {}

    virtual ~TestServer() {
    }

protected:

    // SocketServer::I_Observer implementation:

    void serverConnected(int id) throw () override {
        PRINT("Server connected: " << id);
    }

    void serverReceived(int id, const uint8_t * UNUSED(data), size_t size) throw () override {
        PRINT("Server received bytes: " << size);
        _socket.disconnect(id);
    }

    void serverDisconnected(int id) throw () override {
        PRINT("Server disconnected: " << id);
        ++_count;
    }
};

//
//
//

class TestClient : protected SocketClient::I_Observer {
    SocketClient _socket;

public:
    TestClient(I_Selector & selector, const std::string & path) :
        _socket(*this, selector, path)
    {
        uint8_t byte = 0xFF;
        _socket.send(&byte, 1);
    }

    virtual ~TestClient() {
    }

protected:

    // SocketClient::I_Observer implementation:

    void clientDisconnected() throw () override {
        PRINT("Client disconnected");
    }

    void clientQueueEmpty() throw () override {
        PRINT("Client queue empty");
    }
};

//
//
//

int main() {
    Selector selector;
    auto path = "/tmp/my-socket";
    size_t count = 0;
    TestServer server(selector, path, count);
    TestClient client1(selector, path);
    TestClient client2(selector, path);
    TestClient client3(selector, path);
    TestClient client4(selector, path);
    TestClient client5(selector, path);

    while (count != 5) {
        selector.animate();
    }

    return 0;
}
