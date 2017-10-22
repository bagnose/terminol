// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__NET__HXX
#define SUPPORT__NET__HXX

#include "terminol/support/selector.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/sys.hxx"
#include "terminol/support/pattern.hxx"

#include <set>
#include <deque>
#include <vector>

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

class SocketServer final : protected I_Selector::I_ReadHandler {
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
    SocketServer(I_Observer        & observer,
                 I_Selector        & selector,
                 const std::string & path) :
        _observer(observer),
        _selector(selector),
        _path(path)
    {
        _fd = THROW_IF_SYSCALL_FAILS(::socket(PF_UNIX, SOCK_SEQPACKET, 0), "socket()");
        ScopeGuard fdGuard([this] () { TEMP_FAILURE_RETRY(::close(_fd)); });

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

        THROW_IF_SYSCALL_FAILS(::bind(_fd,
                                      reinterpret_cast<struct sockaddr *>(&address),
                                      sizeof(address)),
                               "Failed to bind to socket " + path);
        THROW_IF_SYSCALL_FAILS(::listen(_fd, 5), "");

        _selector.addReadable(_fd, this);

        fdGuard.dismiss();
    }

    ~SocketServer() {
        for (auto con : _connections) {
            _selector.removeReadable(con);
            TEMP_FAILURE_RETRY(::close(con));
        }

        _selector.removeReadable(_fd);
        TEMP_FAILURE_RETRY(::close(_fd));
    }

    void disconnect(int id) {
        auto fd = id;
        THROW_IF_SYSCALL_FAILS(::shutdown(fd, SHUT_RDWR), "");
    }

protected:
    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) override {
        if (fd == _fd) {
            struct sockaddr_un address;
            ::memset(&address, 0, sizeof address);
            socklen_t length = sizeof address;

            // XXX Can't this fail if the client vanishes during the connect?
            int conFd = THROW_IF_SYSCALL_FAILS(::accept(fd,
                                                        reinterpret_cast<struct sockaddr *>(&address),
                                                        &length),
                                               "accept()");

            _selector.addReadable(conFd, this);
            _connections.insert(conFd);
            _observer.serverConnected(conFd);
        }
        else {
            uint8_t buf[BUFSIZ];
            ssize_t rval = THROW_IF_SYSCALL_FAILS(::recv(fd, buf, sizeof buf, 0), "recv()");

            if (rval == 0) {
                _observer.serverDisconnected(fd);
                _connections.erase(fd);
                _selector.removeReadable(fd);
                TEMP_FAILURE_RETRY(::close(fd));
            }
            else {
                ASSERT(rval > 0, "");
                _observer.serverReceived(fd, buf, rval);
            }
        }
    }
};

//
//
//

class SocketClient final : protected I_Selector::I_WriteHandler {
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
    SocketClient(I_Observer        & observer,
                 I_Selector        & selector,
                 const std::string & path) :
        _observer(observer),
        _selector(selector)
    {
        // FIXME We are using SOCK_SEQPACKET yet _queue collapses the "datagrams"...
        _fd = THROW_IF_SYSCALL_FAILS(::socket(PF_UNIX, SOCK_SEQPACKET, 0), "socket()");
        ScopeGuard fdGuard([this] () { TEMP_FAILURE_RETRY(::close(_fd)); });

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

        // XXX Isn't it possible that we'll get EINPROGRESS ?
        THROW_IF_SYSCALL_FAILS(::connect(_fd,
                                         reinterpret_cast<struct sockaddr *>(&address),
                                         sizeof(address)),
                               "Failed to connect to socket " + path);

        fdGuard.dismiss();
    }

    ~SocketClient() {
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

        ssize_t rval = THROW_IF_SYSCALL_FAILS(::send(fd, &_queue.front(), _queue.size(), MSG_NOSIGNAL),
                                              "send()");

        _queue.erase(_queue.begin(), _queue.begin() + rval);

        if (_queue.empty()) {
            _selector.removeWriteable(fd);
            _observer.clientQueueEmpty();
        }
    }
};

#endif // SUPPORT__NET__HXX
