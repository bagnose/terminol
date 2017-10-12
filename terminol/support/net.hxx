// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__NET__HXX
#define SUPPORT__NET__HXX

#include "terminol/support/selector.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/sys.hxx"

#include <set>
#include <deque>
#include <vector>

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

class SocketServer : protected I_Selector::I_ReadHandler {
public:
    class I_Observer {
    public:
        virtual void serverConnected(int id) = 0;
        virtual void serverReceived(int id, const uint8_t * data, size_t size) = 0;
        virtual void serverDisconnected(int id) = 0;

    protected:
        ~I_Observer() = default;
    };

private:
    I_Observer    & _observer;
    I_Selector    & _selector;
    std::string     _path;
    int             _fd;
    std::set<int>   _connections;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    SocketServer(I_Observer        & observer,
                 I_Selector        & selector,
                 const std::string & path) /*throw (Error)*/ :
        _observer(observer),
        _selector(selector),
        _path(path)
    {
        _fd = ::socket(PF_UNIX, SOCK_SEQPACKET, 0);     // socket() doesn't raise EINTR.
        ENFORCE_SYS(_fd != -1, "Failed to create socket.");

        fdCloseExec(_fd);

        struct sockaddr_un address;
        ::memset(&address, 0, sizeof address);
        address.sun_family  = AF_UNIX;
#ifdef __linux__
        address.sun_path[0] = '\0';     // First byte nul for abstract.
        ::snprintf(address.sun_path + 1, sizeof address.sun_path - 1, "%s", path.c_str());
#else
        ::snprintf(address.sun_path, sizeof address.sun_path, "%s", path.c_str());
#endif

        if (TEMP_FAILURE_RETRY(::bind(_fd, reinterpret_cast<struct sockaddr *>(&address),
                                      sizeof(address))) == -1) {
            ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(_fd)) != -1, "");
            switch (errno) {
                case EADDRINUSE:
                    throw Error("Failed to bind to socket " + path + ": " + std::string(::strerror(errno)));
                default:
                    FATAL("");
            }
        }

        ENFORCE_SYS(::listen(_fd, 5) != -1, "");        // listen() doesn't raise EINTR.

        _selector.addReadable(_fd, this);
    }

    virtual ~SocketServer() {
        for (auto con : _connections) {
            _selector.removeReadable(con);
            ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(con)) != -1, "");
        }

        _selector.removeReadable(_fd);
        ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(_fd)) != -1, "");
    }

    void disconnect(int id) {
        auto fd = id;
        ENFORCE_SYS(::shutdown(fd, SHUT_RDWR) != -1, "");   // shutdown() doesn't raise EINTR.
    }

protected:
    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) override {
        if (fd == _fd) {
            struct sockaddr_un address;
            ::memset(&address, 0, sizeof address);
            socklen_t length = sizeof address;
            auto conFd = TEMP_FAILURE_RETRY(::accept(fd, reinterpret_cast<struct sockaddr *>(&address),
                                                     &length));

            if (conFd == -1) {
                ERROR("Failed to accept connection: " << strerror(errno));
            }
            else {
                _selector.addReadable(conFd, this);
                _connections.insert(conFd);
                _observer.serverConnected(conFd);
            }
        }
        else {
            uint8_t buf[BUFSIZ];
            auto rval = TEMP_FAILURE_RETRY(::recv(fd, buf, sizeof buf, 0));

            if (rval == -1) {
                FATAL("Bad read.");
            }
            else if (rval == 0) {
                _observer.serverDisconnected(fd);
                _connections.erase(fd);
                _selector.removeReadable(fd);
                ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(fd)) != -1, "");
            }
            else {
                _observer.serverReceived(fd, buf, rval);
            }
        }
    }
};

//
//
//

class SocketClient : protected I_Selector::I_WriteHandler {
public:
    class I_Observer {
    public:
        virtual void clientDisconnected() = 0;
        virtual void clientQueueEmpty() = 0;

    protected:
        ~I_Observer() = default;
    };

private:
    I_Observer           & _observer;
    I_Selector           & _selector;
    int                    _fd;
    std::vector<uint8_t>   _queue;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    SocketClient(I_Observer        & observer,
                 I_Selector        & selector,
                 const std::string & path) /*throw (Error)*/ :
        _observer(observer),
        _selector(selector)
    {
        _fd = ::socket(PF_UNIX, SOCK_SEQPACKET, 0);     // socket() doesn't raise EINTR.
        ENFORCE_SYS(_fd != -1, "Failed to create socket.");

        fdCloseExec(_fd);
        fdNonBlock(_fd);

        struct sockaddr_un address;
        ::memset(&address, 0, sizeof address);
        address.sun_family  = AF_UNIX;
#ifdef __linux__
        address.sun_path[0] = '\0';     // First byte nul for abstract.
        ::snprintf(address.sun_path + 1, sizeof address.sun_path - 1, "%s", path.c_str());
#else
        ::snprintf(address.sun_path, sizeof address.sun_path, "%s", path.c_str());
#endif

        if (TEMP_FAILURE_RETRY(::connect(_fd, reinterpret_cast<struct sockaddr *>(&address),
                                         sizeof(address))) == -1) {
            ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(_fd)) != -1, "");
            switch (errno) {
                case ECONNREFUSED:
                case EAGAIN:
                    throw Error("Failed to connect to socket " + path + ": " + std::string(::strerror(errno)));
                default:
                    FATAL("");
            }
        }
    }

    virtual ~SocketClient() {
        if (!_queue.empty()) {
            _selector.removeWriteable(_fd);
        }
        TEMP_FAILURE_RETRY(::close(_fd));
    }

    void send(const uint8_t * data, size_t size) {
        if (_queue.empty()) {
            _selector.addWriteable(_fd, this);
        }

        auto oldSize = _queue.size();
        _queue.resize(oldSize + size);
        std::copy(data, data + size, &_queue[oldSize]);
    }

protected:
    // I_Selector::I_Writer implementation:

    void handleWrite(int fd) override {
        ASSERT(fd == _fd, "");
        ASSERT(!_queue.empty(), "");
        auto rval = TEMP_FAILURE_RETRY(::send(fd, &_queue.front(), _queue.size(), MSG_NOSIGNAL));

        _queue.erase(_queue.begin(), _queue.begin() + rval);

        if (_queue.empty()) {
            _selector.removeWriteable(fd);
            _observer.clientQueueEmpty();
        }
    }
};

#endif // SUPPORT__NET__HXX
