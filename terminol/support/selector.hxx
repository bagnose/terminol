// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__SELECTOR__HXX
#define SUPPORT__SELECTOR__HXX

#include "terminol/support/debug.hxx"

#include <map>
#include <chrono>
#include <algorithm>
#include <vector>

#include <unistd.h>
#include <fcntl.h>

class I_Selector {
public:
    class I_ReadHandler {
    public:
        virtual void handleRead(int fd) = 0;

    protected:
        I_ReadHandler() {}
        ~I_ReadHandler() {}
    };

    class I_WriteHandler {
    public:
        virtual void handleWrite(int fd) = 0;

    protected:
        I_WriteHandler() {}
        ~I_WriteHandler() {}
    };

    class I_TimeoutHandler {
    public:
        virtual void handleTimeout() = 0;

    protected:
        I_TimeoutHandler() {}
        ~I_TimeoutHandler() {}
    };

    virtual void addReadable(int fd, I_ReadHandler * handler) = 0;
    virtual void removeReadable(int fd) = 0;
    virtual void addWriteable(int fd, I_WriteHandler * handler) = 0;
    virtual void removeWriteable(int fd) = 0;
    virtual void addTimeoutable(I_TimeoutHandler * handler, int milliseconds) = 0;
    virtual void removeTimeoutable(I_TimeoutHandler * handler) = 0;

protected:
    I_Selector() {}
    ~I_Selector() {}
};

#ifndef __linux__

#include <sys/select.h>

//
//
//

class SelectSelector : public I_Selector {
    typedef std::chrono::steady_clock Clock;

    struct TimeEntry {
        TimeEntry(Clock::time_point  time_, I_TimeoutHandler * handler_) :
            time(time_), handler(handler_) {}

        Clock::time_point   time;
        I_TimeoutHandler  * handler;
    };

    std::map<int, I_ReadHandler *>  _readRegs;
    std::map<int, I_WriteHandler *> _writeRegs;
    std::vector<TimeEntry>          _timeoutRegs;

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
            auto fd = reg.first;
            FD_SET(fd, &readFds);
            max = std::max(max, fd);
        }

        for (auto reg : _writeRegs) {
            auto fd = reg.first;
            FD_SET(fd, &writeFds);
            max = std::max(max, fd);
        }

        int n;

        if (!_timeoutRegs.empty()) {
            auto now  = Clock::now();
            auto next = std::max(now, _timeoutRegs.front().time);

            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(next - now);

            const int A_MILLION = 1000000;
            struct timeval tv;
            tv.tv_sec  = duration.count() / A_MILLION;
            tv.tv_usec = duration.count() % A_MILLION;

            n = TEMP_FAILURE_RETRY(::select(max + 1, &readFds, &writeFds, nullptr, &tv));
        }
        else {
            n = TEMP_FAILURE_RETRY(::select(max + 1, &readFds, &writeFds, nullptr, nullptr));
            ENFORCE_SYS(n != 0, "");
        }

        ENFORCE_SYS(n != -1, "");

        if (n == 0) {
            ASSERT(!_timeoutRegs.empty(), "");

            auto now = Clock::now();

            while (!_timeoutRegs.empty()) {
                auto & reg = _timeoutRegs.back();

                if (now < reg.time) {
                    break;
                }

                auto handler = reg.handler;
                _timeoutRegs.pop_back();    // Invalidates reg reference.

                handler->handleTimeout();
            }
        }
        else {
            for (auto iter = _readRegs.begin(); iter != _readRegs.end(); /**/) {
                auto fd      = iter->first;
                auto handler = iter->second;
                ++iter;

                if (FD_ISSET(fd, &readFds)) {
                    handler->handleRead(fd);
                }
            }

            for (auto iter = _writeRegs.begin(); iter != _writeRegs.end(); /**/) {
                auto fd      = iter->first;
                auto handler = iter->second;
                ++iter;

                if (FD_ISSET(fd, &writeFds)) {
                    handler->handleWrite(fd);
                }
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        ASSERT(_readRegs.find(fd) == _readRegs.end(), "");
        _readRegs.insert(std::make_pair(fd, handler));
    }

    void removeReadable(int fd) {
        auto iter = _readRegs.find(fd);
        ASSERT(iter != _readRegs.end(), "");
        _readRegs.erase(iter);
    }

    void addWriteable(int fd, I_WriteHandler * handler) {
        ASSERT(_writeRegs.find(fd) == _writeRegs.end(), "");
        _writeRegs.insert(std::make_pair(fd, handler));
    }

    void removeWriteable(int fd) {
        auto iter = _writeRegs.find(fd);
        ASSERT(iter != _writeRegs.end(), "");
        _writeRegs.erase(iter);
    }

    void addTimeoutable(I_TimeoutHandler * handler, int milliseconds) {
#if DEBUG
        for (auto & reg : _timeoutRegs) {
            ASSERT(reg.handler != handler, "Handler already registered.");
        }
#endif
        Clock::time_point scheduled =
            Clock::now() + std::chrono::duration<int, std::milli>(milliseconds);

        auto iter = _timeoutRegs.begin();

        while (iter != _timeoutRegs.end()) {
            if (iter->time < scheduled) {
                break;
            }

            ++iter;
        }

        _timeoutRegs.insert(iter, TimeEntry(scheduled, handler));
    }

    void removeTimeoutable(I_TimeoutHandler * handler) {
        for (auto iter = _timeoutRegs.begin(); iter != _timeoutRegs.end(); ++iter) {
            if (iter->handler == handler) {
                _timeoutRegs.erase(iter);
                return;
            }
        }

        FATAL("Handler not registered.");
    }
};

typedef SelectSelector Selector;

#else // __linux__

#include <sys/epoll.h>

//
//
//

class EPollSelector : public I_Selector {
    typedef std::chrono::steady_clock Clock;

    struct TimeEntry {
        TimeEntry(Clock::time_point  time_, I_TimeoutHandler * handler_) :
            time(time_), handler(handler_) {}

        Clock::time_point   time;
        I_TimeoutHandler  * handler;
    };

    int                             _fd;
    std::map<int, I_ReadHandler *>  _readRegs;
    std::map<int, I_WriteHandler *> _writeRegs;
    std::vector<TimeEntry>          _timeoutRegs;

public:
    EPollSelector() {
        _fd = ::epoll_create1(EPOLL_CLOEXEC);
    }

    virtual ~EPollSelector() {
        ENFORCE_SYS(::close(_fd) != -1, "");
    }

    void animate() {
        ASSERT(!_readRegs.empty() || !_writeRegs.empty(), "");

        const int MAX_EVENTS = 8;
        struct epoll_event event_array[MAX_EVENTS];

        int n;

        if (!_timeoutRegs.empty()) {
            auto now  = Clock::now();
            auto next = std::max(now, _timeoutRegs.front().time);

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
            auto timeout  = duration.count();

            n = TEMP_FAILURE_RETRY(::epoll_wait(_fd, event_array, MAX_EVENTS, timeout));
        }
        else {
            n = TEMP_FAILURE_RETRY(::epoll_wait(_fd, event_array, MAX_EVENTS, -1));
            ENFORCE_SYS(n != 0, "");
        }

        ENFORCE_SYS(n != -1, "");

        if (n == 0) {
            ASSERT(!_timeoutRegs.empty(), "");

            auto now = Clock::now();

            while (!_timeoutRegs.empty()) {
                auto & reg = _timeoutRegs.back();

                if (now < reg.time) {
                    break;
                }

                auto handler = reg.handler;
                _timeoutRegs.pop_back();    // Invalidates reg reference.

                handler->handleTimeout();
            }
        }
        else {
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
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) {
        ASSERT(_readRegs.find(fd) == _readRegs.end(), "");

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_writeRegs.find(fd) == _writeRegs.end()) {
            event.events = EPOLLIN;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");     // epoll_ctl() doesn't raise EINTR.
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");     // epoll_ctl() doesn't raise EINTR.
        }

        _readRegs.insert(std::make_pair(fd, handler));
    }

    void removeReadable(int fd) {
        auto iter = _readRegs.find(fd);
        ASSERT(iter != _readRegs.end(), "");

        if (_writeRegs.find(fd) == _writeRegs.end()) {
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");        // epoll_ctl() doesn't raise EINTR.
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLOUT;
            event.data.fd = fd;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");     // epoll_ctl() doesn't raise EINTR.
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
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event) != -1, "");     // epoll_ctl() doesn't raise EINTR.
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");     // epoll_ctl() doesn't raise EINTR.
        }

        _writeRegs.insert(std::make_pair(fd, handler));
    }

    void removeWriteable(int fd) {
        auto iter = _writeRegs.find(fd);
        ASSERT(iter != _writeRegs.end(), "");

        if (_readRegs.find(fd) == _readRegs.end()) {
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr) != -1, "");        // epoll_ctl() doesn't raise EINTR.
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLIN;
            event.data.fd = fd;
            ENFORCE_SYS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event) != -1, "");     // epoll_ctl() doesn't raise EINTR.
        }

        _writeRegs.erase(iter);
    }

    void addTimeoutable(I_TimeoutHandler * handler, int milliseconds) {
#if DEBUG
        for (auto & reg : _timeoutRegs) {
            ASSERT(reg.handler != handler, "Handler already registered.");
        }
#endif
        Clock::time_point scheduled =
            Clock::now() + std::chrono::duration<int,std::milli>(milliseconds);

        auto iter = _timeoutRegs.begin();

        while (iter != _timeoutRegs.end()) {
            if (iter->time < scheduled) {
                break;
            }

            ++iter;
        }

        _timeoutRegs.insert(iter, TimeEntry(scheduled, handler));
    }

    void removeTimeoutable(I_TimeoutHandler * handler) {
        for (auto iter = _timeoutRegs.begin(); iter != _timeoutRegs.end(); ++iter) {
            if (iter->handler == handler) {
                _timeoutRegs.erase(iter);
                return;
            }
        }

        FATAL("Handler not registered.");
    }
};

typedef EPollSelector Selector;

#endif // __linux__

#endif // SUPPORT__SELECTOR__HXX
