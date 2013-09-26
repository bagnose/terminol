// vi:noai:sw=4

#ifndef SUPPORT__SELECTOR__HXX
#define SUPPORT__SELECTOR__HXX

#include "terminol/support/debug.hxx"

#include <vector>
#include <algorithm>

#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/epoll.h>

class I_Selector {
public:
    class I_ReadHandler {
    public:
        virtual void handleRead(int fd) throw () = 0;

    protected:
        I_ReadHandler() {}
        ~I_ReadHandler() {}
    };

    class I_WriteHandler {
    public:
        virtual void handleWrite(int fd) throw () = 0;

    protected:
        I_WriteHandler() {}
        ~I_WriteHandler() {}
    };

    virtual void addReadable(int fd, I_ReadHandler * handler) = 0;
    virtual void removeReadable(int fd) = 0;
    virtual void addWriteable(int fd, I_WriteHandler * handler) = 0;
    virtual void removeWriteable(int fd) = 0;

protected:
    I_Selector() {}
    ~I_Selector() {}
};

//
//
//

class SelectSelector : public I_Selector {
    struct ReadReg {
        ReadReg(int fd_, I_ReadHandler * handler_) : fd(fd_), handler(handler_) {}
        int             fd;
        I_ReadHandler * handler;
    };

    struct WriteReg {
        WriteReg(int fd_, I_WriteHandler * handler_) : fd(fd_), handler(handler_) {}
        int             fd;
        I_WriteHandler * handler;
    };

    std::vector<ReadReg>  _readRegs;
    std::vector<WriteReg> _writeRegs;

public:
    SelectSelector() {}

    virtual ~SelectSelector() {
        ASSERT(_readRegs.empty(), "");
        ASSERT(_writeRegs.empty(), "");
    }

    void animate() {
        ASSERT(!_readRegs.empty() || !_writeRegs.empty(), "");

        fd_set readFds;
        fd_set writeFds;
        int max = -1;

        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);

        for (auto reg : _readRegs) {
            FD_SET(reg.fd, &readFds);
            max = std::max(max, reg.fd);
        }

        for (auto reg : _writeRegs) {
            FD_SET(reg.fd, &writeFds);
            max = std::max(max, reg.fd);
        }

        //PRINT("Selecting, max=" << max);
        ENFORCE_SYS(TEMP_FAILURE_RETRY(
            ::select(max + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");

        // Copy the vectors in case they change during dispatch.

        auto readRegsCopy = _readRegs;
        for (auto reg : readRegsCopy) {
            if (FD_ISSET(reg.fd, &readFds)) {
                //PRINT("readable: " << reg.fd);
                reg.handler->handleRead(reg.fd);
            }
        }

        auto writeRegsCopy = _writeRegs;
        for (auto reg : writeRegsCopy) {
            if (FD_ISSET(reg.fd, &writeFds)) {
                //PRINT("writeable: " << reg.fd);
                reg.handler->handleWrite(reg.fd);
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        //PRINT("Adding readable: " << fd);
        ASSERT(std::find_if(_readRegs.begin(), _readRegs.end(),
                            [fd](ReadReg & reg) { return reg.fd == fd; }) == _readRegs.end(), "");
        _readRegs.push_back(ReadReg(fd, handler));
    }

    void removeReadable(int fd) {
        //PRINT("Removing readable: " << fd);
        auto iter = std::find_if(_readRegs.begin(), _readRegs.end(),
                                 [fd](ReadReg & reg) { return reg.fd == fd; });
        ASSERT(iter != _readRegs.end(), "");
        _readRegs.erase(iter);
    }

    void addWriteable(int fd, I_WriteHandler * handler) {
        //PRINT("Adding writeable: " << fd);
        ASSERT(std::find_if(_writeRegs.begin(), _writeRegs.end(),
                            [fd](WriteReg & reg) { return reg.fd == fd; }) == _writeRegs.end(), "");
        _writeRegs.push_back(WriteReg(fd, handler));
    }

    void removeWriteable(int fd) {
        //PRINT("Removing writeable: " << fd);
        auto iter = std::find_if(_writeRegs.begin(), _writeRegs.end(),
                                 [fd](WriteReg & reg) { return reg.fd == fd; });
        ASSERT(iter != _writeRegs.end(), "");
        _writeRegs.erase(iter);
    }
};

//
//
//

class EPollSelector : public I_Selector {
    int                             _fd;
    std::map<int, I_ReadHandler *>  _readRegs;
    std::map<int, I_WriteHandler *> _writeRegs;

public:
    EPollSelector() {
        _fd = ::epoll_create1(0);
        ENFORCE_SYS(_fd != -1, "");
        int flags;
        ENFORCE_SYS((flags = ::fcntl(_fd, F_GETFD)) != -1, "");
        flags |= FD_CLOEXEC;
        ENFORCE_SYS(::fcntl(_fd, F_SETFD, flags) != -1, "");
    }

    virtual ~EPollSelector() {
        ENFORCE_SYS(::close(_fd) != -1, "");
    }

    void animate() {
        const int MAX_EVENTS = 8;
        struct epoll_event event_array[MAX_EVENTS];

        auto n = TEMP_FAILURE_RETRY(::epoll_wait(_fd, event_array, MAX_EVENTS, -1));
        ENFORCE_SYS(n != -1, "");
        ENFORCE(n > 0, "");

        //PRINT("Got: " << n << " events");

        for (auto i = 0; i != n; ++i) {
            auto & event = event_array[i];

            auto fd     = event.data.fd;
            auto events = event.events;

            if (events & EPOLLERR) {
                ERROR("Error on fd: " << fd);
            }

            if (events & (EPOLLHUP | EPOLLIN)) {
                auto iter = _readRegs.find(fd);
                ASSERT(iter != _readRegs.end(), "");
                auto handler = iter->second;
                handler->handleRead(fd);
            }

            if (events & EPOLLOUT) {
                auto iter = _writeRegs.find(fd);
                ASSERT(iter != _writeRegs.end(), "");
                auto handler = iter->second;
                handler->handleWrite(fd);
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        ASSERT(_readRegs.find(fd) == _readRegs.end(), "");

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_writeRegs.find(fd) == _writeRegs.end()) {
            event.events = EPOLLIN;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _readRegs.insert(std::make_pair(fd, handler));
    }

    void removeReadable(int fd) {
        auto iter = _readRegs.find(fd);
        ASSERT(iter != _readRegs.end(), "");

        if (_writeRegs.find(fd) == _writeRegs.end()) {
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLOUT;
            event.data.fd = fd;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _readRegs.erase(iter);
    }

    void addWriteable(int fd, I_WriteHandler * handler) {
        ASSERT(_writeRegs.find(fd) == _writeRegs.end(), "");

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_readRegs.find(fd) == _readRegs.end()) {
            event.events = EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _writeRegs.insert(std::make_pair(fd, handler));
    }

    void removeWriteable(int fd) {
        auto iter = _writeRegs.find(fd);
        ASSERT(iter != _writeRegs.end(), "");

        if (_readRegs.find(fd) == _readRegs.end()) {
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLIN;
            event.data.fd = fd;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _writeRegs.erase(iter);
    }
};

//
//
//

//typedef SelectSelector Selector;
typedef EPollSelector Selector;

#endif // SUPPORT__SELECTOR__HXX
