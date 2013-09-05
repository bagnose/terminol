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

    virtual void addReadable(int fd, I_ReadHandler * handler) = 0;
    virtual void removeReadable(int fd) = 0;

protected:
    I_Selector() {}
    ~I_Selector() {}
};

//
//
//

class SelectSelector : public I_Selector {
    struct Reg {
        Reg(int fd_, I_ReadHandler * handler_) : fd(fd_), handler(handler_) {}
        int             fd;
        I_ReadHandler * handler;
    };

    std::vector<Reg> _regs;

public:
    SelectSelector() {}

    virtual ~SelectSelector() {
        ASSERT(_regs.empty(), "");
    }

    void animate() {
        ASSERT(!_regs.empty(), "");

        fd_set readFds;
        int max = -1;

        FD_ZERO(&readFds);

        for (auto reg : _regs) {
            FD_SET(reg.fd, &readFds);
            max = std::max(max, reg.fd);
        }

        //PRINT("Selecting, max=" << max);
        ENFORCE_SYS(TEMP_FAILURE_RETRY(
            ::select(max + 1, &readFds, nullptr, nullptr, nullptr)) != -1, "");

        // Copy the vector in case it changes during dispatch.
        auto regsCopy = _regs;
        for (auto reg : regsCopy) {
            if (FD_ISSET(reg.fd, &readFds)) {
                //PRINT("readable: " << reg.fd);
                reg.handler->handleRead(reg.fd);
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        //PRINT("Adding readable: " << fd);
        ASSERT(std::find_if(_regs.begin(), _regs.end(),
                            [fd](Reg reg) { return reg.fd == fd; }) == _regs.end(), "");
        _regs.push_back(Reg(fd, handler));
    }

    void removeReadable(int fd) {
        //PRINT("Removing readable: " << fd);
        auto iter = std::find_if(_regs.begin(), _regs.end(),
                                 [fd](Reg reg) { return reg.fd == fd; });
        ASSERT(iter != _regs.end(), "");
        _regs.erase(iter);
    }
};

//
//
//

class EPollSelector : public I_Selector {
    int                            _fd;
    std::map<int, I_ReadHandler *> _regs;

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

        //PRINT("Got: " << n << " events");

        for (auto i = 0; i != n; ++i) {
            auto & event = event_array[i];

            auto fd     = event.data.fd;
            auto events = event.events;

            if (events & EPOLLERR) {
                ERROR("Error on fd: " << fd);
            }

            if (events & (EPOLLHUP | EPOLLIN)) {
                auto iter = _regs.find(fd);
                ASSERT(iter != _regs.end(), "");
                auto handler = iter->second;
                handler->handleRead(fd);
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        ASSERT(_regs.find(fd) == _regs.end(), "");

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        event.events = EPOLLIN;
        ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");
        _regs.insert(std::make_pair(fd, handler));
    }

    void removeReadable(int fd) {
        auto iter = _regs.find(fd);
        ASSERT(iter != _regs.end(), "");

        ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");
        _regs.erase(iter);
    }
};

//
//
//

//typedef SelectSelector Selector;
typedef EPollSelector Selector;

#endif // SUPPORT__SELECTOR__HXX
