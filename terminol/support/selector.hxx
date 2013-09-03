// vi:noai:sw=4

#ifndef SUPPORT__SELECTOR__HXX
#define SUPPORT__SELECTOR__HXX

#include "terminol/support/debug.hxx"

#include <vector>
#include <algorithm>

#include <unistd.h>
#include <sys/select.h>
#include <sys/epoll.h>

class I_ReadHandler {
public:
    virtual void handleRead(int fd) = 0;

protected:
    I_ReadHandler() {}
    ~I_ReadHandler() {}
};

//
//
//

class I_WriteHandler {
public:
    virtual void handleWrite(int fd) = 0;

protected:
    I_WriteHandler() {}
    ~I_WriteHandler() {}
};

//
//
//

class I_Selector {
public:
    virtual void addReadable(int fd, I_ReadHandler * handler) = 0;
    virtual void addWriteable(int fd, I_WriteHandler * handler) = 0;
    virtual void removeReadable(int fd) = 0;
    virtual void removeWriteable(int fd) = 0;

protected:
    I_Selector() {}
    ~I_Selector() {}
};

//
//
//

class SelectSelector : public I_Selector {
    struct RReg {
        RReg(int fd_, I_ReadHandler * handler_) : fd(fd_), handler(handler_) {}
        int             fd;
        I_ReadHandler * handler;
    };

    struct WReg {
        WReg(int fd_, I_WriteHandler * handler_) : fd(fd_), handler(handler_) {}
        int              fd;
        I_WriteHandler * handler;
    };

    std::vector<RReg> _rreg;
    std::vector<WReg> _wreg;

public:
    SelectSelector() {}

    virtual ~SelectSelector() {
        ASSERT(_rreg.empty(), "");
        ASSERT(_wreg.empty(), "");
    }

    void animate() {
        ASSERT(!_rreg.empty() || !_wreg.empty(), "");

        fd_set readFds, writeFds;
        int max = -1;

        FD_ZERO(&readFds);
        FD_ZERO(&writeFds);

        for (auto reg : _rreg) {
            FD_SET(reg.fd, &readFds);
            max = std::max(max, reg.fd);
        }

        for (auto reg : _wreg) {
            FD_SET(reg.fd, &writeFds);
            max = std::max(max, reg.fd);
        }

        //PRINT("Selecting, max=" << max);
        ENFORCE_SYS(TEMP_FAILURE_RETRY(
            ::select(max + 1, &readFds, &writeFds, nullptr, nullptr)) != -1, "");

        {
            // Copy the vector in case it changes during dispatch.
            auto rreg = _rreg;
            for (auto reg : rreg) {
                if (FD_ISSET(reg.fd, &readFds)) {
                    //PRINT("readable: " << reg.fd);
                    reg.handler->handleRead(reg.fd);
                }
            }
        }

        {
            // Copy the vector in case it changes during dispatch.
            auto wreg = _wreg;
            for (auto reg : wreg) {
                if (FD_ISSET(reg.fd, &writeFds)) {
                    //PRINT("writeable: " << reg.fd);
                    reg.handler->handleWrite(reg.fd);
                }
            }
        }
    }

    // I_Selector overrides:

    void addReadable(int fd, I_ReadHandler * handler) {
        //PRINT("Adding readable: " << fd);
        ASSERT(std::find_if(_rreg.begin(), _rreg.end(),
                            [fd](RReg rreg) { return rreg.fd == fd; }) == _rreg.end(), "");
        _rreg.push_back(RReg(fd, handler));
    }

    void addWriteable(int fd, I_WriteHandler * handler) {
        //PRINT("Adding writeable: " << fd);
        ASSERT(std::find_if(_wreg.begin(), _wreg.end(),
                            [fd](WReg wreg) { return wreg.fd == fd; }) == _wreg.end(), "");
        _wreg.push_back(WReg(fd, handler));
    }

    void removeReadable(int fd) {
        //PRINT("Removing readable: " << fd);
        auto iter = std::find_if(_rreg.begin(), _rreg.end(),
                                 [fd](RReg rreg) { return rreg.fd == fd; });
        ASSERT(iter != _rreg.end(), "");
        _rreg.erase(iter);
    }

    void removeWriteable(int fd) {
        //PRINT("Removing writeable: " << fd);
        auto iter = std::find_if(_wreg.begin(), _wreg.end(),
                                 [fd](WReg wreg) { return wreg.fd == fd; });
        ASSERT(iter != _wreg.end(), "");
        _wreg.erase(iter);
    }
};

//
//
//

class EPollSelector : public I_Selector {
    int                             _fd;
    std::map<int, I_ReadHandler *>  _readers;
    std::map<int, I_WriteHandler *> _writers;

public:
    EPollSelector() {
        _fd = ::epoll_create1(0);
        ENFORCE_SYS(_fd != -1, "");
    }

    virtual ~EPollSelector() {
        ENFORCE_SYS(::close(_fd) != -1, "");
    }

    void animate() {
        const int MAX_EVENTS = 8;
        struct epoll_event event_array[MAX_EVENTS];

        auto n = TEMP_FAILURE_RETRY(::epoll_wait(_fd, event_array, MAX_EVENTS, -1));
        ENFORCE_SYS(n != -1, "");

        //PRINT("Got: " << n << " events");

        for (auto i = 0; i != n; ++i) {
            auto & event = event_array[i];

            auto fd     = event.data.fd;
            auto events = event.events;

            if (events & EPOLLERR) {
                ERROR("Error on fd: " << fd);
            }

            if (events & (EPOLLHUP | EPOLLIN)) {
                auto iter = _readers.find(fd);
                ASSERT(iter != _readers.end(), "");
                auto handler = iter->second;
                handler->handleRead(fd);
            }

            if (events & EPOLLOUT) {
                auto iter = _writers.find(fd);
                ASSERT(iter != _writers.end(), "");
                auto handler = iter->second;
                handler->handleWrite(fd);
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        ASSERT(_readers.find(fd) == _readers.end(), "");

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_writers.find(fd) == _writers.end()) {
            event.events = EPOLLIN;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _readers.insert(std::make_pair(fd, handler));
    }

    void addWriteable(int fd, I_WriteHandler * handler) {
        ASSERT(_writers.find(fd) == _writers.end(), "");

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_readers.find(fd) == _readers.end()) {
            event.events = EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _writers.insert(std::make_pair(fd, handler));
    }

    void removeReadable(int fd) {
        auto iter = _readers.find(fd);
        ASSERT(iter != _readers.end(), "");

        if (_writers.find(fd) == _writers.end()) {
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLOUT;
            event.data.fd = fd;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _readers.erase(iter);
    }

    void removeWriteable(int fd) {
        auto iter = _writers.find(fd);
        ASSERT(iter != _writers.end(), "");

        if (_readers.find(fd) == _readers.end()) {
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLIN;
            event.data.fd = fd;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");
        }

        _writers.erase(iter);
    }
};

//
//
//

//typedef SelectSelector Selector;
typedef EPollSelector Selector;

#endif // SUPPORT__SELECTOR__HXX
