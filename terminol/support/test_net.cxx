// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/support/net.hxx"

class TestServer final : private SocketServer::I_Observer {
    SocketServer _socket;
    size_t       _count = 0;

public:
    TestServer(I_Selector & selector, const std::string & path) : _socket(*this, selector, path) {}

    size_t getCount() const { return _count; }

private:
    // SocketServer::I_Observer implementation:

    void serverConnected(int id) override { PRINT("Server connected: " << id); }

    void serverReceived(int id, const uint8_t * UNUSED(data), size_t size) override {
        PRINT("Server received bytes: " << size);
        _socket.disconnect(id);
    }

    void serverDisconnected(int id) override {
        PRINT("Server disconnected: " << id);
        ++_count;
    }
};

//
//
//

class TestClient final : private SocketClient::I_Observer {
    SocketClient _socket;

public:
    TestClient(I_Selector & selector, const std::string & path) : _socket(*this, selector, path) {
        uint8_t byte = 0xFF;
        _socket.send(&byte, 1);
    }

private:
    // SocketClient::I_Observer implementation:

    void clientDisconnected() override { PRINT("Client disconnected"); }

    void clientQueueEmpty() override { PRINT("Client queue empty"); }
};

//
//
//

int main() {
    Selector   selector;
    auto       path = "/tmp/my-socket";
    TestServer server(selector, path);
    TestClient client1(selector, path);
    TestClient client2(selector, path);
    TestClient client3(selector, path);
    TestClient client4(selector, path);
    TestClient client5(selector, path);

    while (server.getCount() != 5) { selector.animate(); }

    return 0;
}
