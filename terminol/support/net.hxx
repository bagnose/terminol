// vi:noai:sw=4

#ifndef SUPPORT__NET__HXX
#define SUPPORT__NET__HXX

#include "terminol/support/selector.hxx"
#include "terminol/support/debug.hxx"

#include <set>
#include <deque>

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/un.h>

#define UNIX_PATH_MAX 108           // XXX Why isn't this defined??

class SocketServer : protected I_Selector::I_ReadHandler {
public:
    class I_Observer {
    public:
        virtual void serverConnected(int id) throw () = 0;
        virtual void serverReceived(int id, const uint8_t * data, size_t size) throw () = 0;
        virtual void serverDisconnected(int id) throw () = 0;

    protected:
        ~I_Observer() {}
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
                 const std::string & path) throw (Error) :
        _observer(observer),
        _selector(selector),
        _path(path)
    {
        _fd = TEMP_FAILURE_RETRY(::socket(PF_UNIX, SOCK_SEQPACKET, 0));
        ENFORCE_SYS(_fd != -1, "Failed to create socket.");

        struct sockaddr_un address;
        ::memset(&address, 0, sizeof address);
        address.sun_family  = AF_UNIX;
        address.sun_path[0] = '\0';     // First byte nul for abstract.
        ::snprintf(address.sun_path + 1, UNIX_PATH_MAX - 1, "%s", path.c_str());

        if (TEMP_FAILURE_RETRY(::bind(_fd, reinterpret_cast<struct sockaddr *>(&address),
                                      sizeof(address))) == -1) {
            ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(_fd)) != -1, "");
            switch (errno) {
                case EADDRINUSE:
                    throw Error("Failed to bind to socket " + path + ": " + std::string(::strerror(errno)));
                default:
                    ENFORCE_SYS(false, "");
            }
        }

        ENFORCE_SYS(TEMP_FAILURE_RETRY(::listen(_fd, 5)) != -1, "");

        _selector.addReadable(_fd, this);
    }

    virtual ~SocketServer() {
        for (auto con : _connections) {
            _selector.removeReadable(con);
            ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(con)) != -1, "");
        }

        _selector.removeReadable(_fd);
        ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(_fd)) != -1, "");

        if (TEMP_FAILURE_RETRY(::unlink(_path.c_str())) == -1) {
            ERROR("Failed to unlink " << _path << ": " << ::strerror(errno));
        }
    }

    void disconnect(int id) {
        auto fd = id;
        ENFORCE_SYS(TEMP_FAILURE_RETRY(::shutdown(fd, SHUT_RDWR)) != -1, "");
    }

protected:
    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) throw () {
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
            auto rval = TEMP_FAILURE_RETRY(::read(fd, buf, sizeof buf));

            if (rval == -1) {
                FATAL("Bad read");
            }
            else if (rval == 0) {
                _observer.serverDisconnected(fd);
                _connections.erase(fd);
                _selector.removeReadable(fd);
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
        virtual void clientDisconnected() throw () = 0;
        virtual void clientQueueEmpty() throw () = 0;

    protected:
        ~I_Observer() {}
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
                 const std::string & path) throw (Error) :
        _observer(observer),
        _selector(selector)
    {
        _fd = TEMP_FAILURE_RETRY(::socket(PF_UNIX, SOCK_SEQPACKET, 0));
        ENFORCE_SYS(_fd != -1, "Failed to create socket.");

        struct sockaddr_un address;
        ::memset(&address, 0, sizeof address);
        address.sun_family  = AF_UNIX;
        address.sun_path[0] = '\0';     // First byte nul for abstract.
        ::snprintf(address.sun_path + 1, UNIX_PATH_MAX - 1, "%s", path.c_str());

        auto saved_flags = ::fcntl(_fd, F_GETFL);
        ::fcntl(_fd, F_SETFL, saved_flags | O_NONBLOCK);

        if (TEMP_FAILURE_RETRY(::connect(_fd, reinterpret_cast<struct sockaddr *>(&address),
                                         sizeof(address))) == -1) {
            ENFORCE_SYS(TEMP_FAILURE_RETRY(::close(_fd)) != -1, "");
            switch (errno) {
                case ECONNREFUSED:
                case EAGAIN:
                    throw Error("Failed to connect to socket " + path + ": " + std::string(::strerror(errno)));
                default:
                    ENFORCE_SYS(false, "");
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

    void handleWrite(int fd) throw () {
        ASSERT(fd == _fd, "");
        ASSERT(!_queue.empty(), "");
        auto rval = TEMP_FAILURE_RETRY(::write(fd, &_queue.front(), _queue.size()));

        _queue.erase(_queue.begin(), _queue.begin() + rval);

        if (_queue.empty()) {
            _selector.removeWriteable(fd);
            _observer.clientQueueEmpty();
        }
    }
};

#endif // SUPPORT__NET__HXX