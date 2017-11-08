// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__SELECTOR__HXX
#define SUPPORT__SELECTOR__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/exception.hxx"

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
        ~I_ReadHandler() = default;
    };

    class I_WriteHandler {
    public:
        virtual void handleWrite(int fd) = 0;

    protected:
        ~I_WriteHandler() = default;
    };

    class I_TimeoutHandler {
    public:
        virtual void handleTimeout() = 0;

    protected:
        ~I_TimeoutHandler() = default;
    };

    virtual void addReadable(int fd, I_ReadHandler * handler) = 0;
    virtual void removeReadable(int fd) = 0;
    virtual void addWriteable(int fd, I_WriteHandler * handler) = 0;
    virtual void removeWriteable(int fd) = 0;
    virtual void addTimeoutable(I_TimeoutHandler * handler, int milliseconds) = 0;
    virtual void removeTimeoutable(I_TimeoutHandler * handler) = 0;

protected:
    ~I_Selector() = default;
};

#ifndef __linux__

#include <sys/select.h>

//
//
//

class SelectSelector final : public I_Selector {
    using Clock = std::chrono::steady_clock;

    struct TimeEntry {
        Clock::time_point   time;
        I_TimeoutHandler  * handler = nullptr;
    };

    std::map<int, I_ReadHandler *>  _readRegs;
    std::map<int, I_WriteHandler *> _writeRegs;
    std::vector<TimeEntry>          _timeoutRegs;

public:
    SelectSelector() {}

    ~SelectSelector() {
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

            const int A_MILLION = 1'000'000;
            struct timeval tv;
            tv.tv_sec  = duration.count() / A_MILLION;
            tv.tv_usec = duration.count() % A_MILLION;

            n = THROW_IF_SYSCALL_FAILS(::select(max + 1, &readFds, &writeFds, nullptr, &tv), "select()");
        }
        else {
            n = THROW_IF_SYSCALL_FAILS(::select(max + 1, &readFds, &writeFds, nullptr, nullptr), "select()");
        }

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
        _readRegs.insert({fd, handler});
    }

    void removeReadable(int fd) {
        auto iter = _readRegs.find(fd);
        ASSERT(iter != _readRegs.end(), "");
        _readRegs.erase(iter);
    }

    void addWriteable(int fd, I_WriteHandler * handler) {
        ASSERT(_writeRegs.find(fd) == _writeRegs.end(), "");
        _writeRegs.insert({fd, handler});
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

using Selector = SelectSelector;

#else // __linux__

#include <sys/epoll.h>

//
//
//

class EPollSelector final : public I_Selector {
    using Clock = std::chrono::steady_clock;

    struct TimeEntry {
        Clock::time_point   time;
        I_TimeoutHandler  * handler = nullptr;
    };

    int                             _fd;
    std::map<int, I_ReadHandler *>  _readRegs;
    std::map<int, I_WriteHandler *> _writeRegs;
    std::vector<TimeEntry>          _timeoutRegs;

public:
    EPollSelector() {
        _fd = THROW_IF_SYSCALL_FAILS(::epoll_create1(EPOLL_CLOEXEC), "");
    }

    ~EPollSelector() {
        TEMP_FAILURE_RETRY(::close(_fd));
    }

    void animate() {
        ASSERT(!_readRegs.empty() || !_writeRegs.empty(), );

        const int MAX_EVENTS = 8;
        struct epoll_event event_array[MAX_EVENTS];

        int n;

        if (!_timeoutRegs.empty()) {
            auto now  = Clock::now();
            auto next = std::max(now, _timeoutRegs.front().time);

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(next - now);
            auto timeout  = duration.count();

            n = THROW_IF_SYSCALL_FAILS(::epoll_wait(_fd, event_array, MAX_EVENTS, timeout), "epoll_wait()");
        }
        else {
            n = THROW_IF_SYSCALL_FAILS(::epoll_wait(_fd, event_array, MAX_EVENTS, -1), "epoll_wait()");
        }

        if (n == 0) {
            ASSERT(!_timeoutRegs.empty(), );

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
                    ASSERT(iter != _readRegs.end(), );
                    auto handler = iter->second;
                    handler->handleRead(fd);
                }

                if (events & EPOLLOUT) {
                    auto iter = _writeRegs.find(fd);
                    ASSERT(iter != _writeRegs.end(), );
                    auto handler = iter->second;
                    handler->handleWrite(fd);
                }
            }
        }
    }

    // I_Selector implementation:

    void addReadable(int fd, I_ReadHandler * handler) override {
        ASSERT(_readRegs.find(fd) == _readRegs.end(), );

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_writeRegs.find(fd) == _writeRegs.end()) {
            event.events = EPOLLIN;
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event), "epoll_ctl()");
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event), "epoll_ctl()");
        }

        _readRegs.insert({fd, handler});
    }

    void removeReadable(int fd) override {
        auto iter = _readRegs.find(fd);
        ASSERT(iter != _readRegs.end(), );

        if (_writeRegs.find(fd) == _writeRegs.end()) {
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr), "epoll_ctl()");
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLOUT;
            event.data.fd = fd;
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event), "epoll_ctl()");
        }

        _readRegs.erase(iter);
    }

    void addWriteable(int fd, I_WriteHandler * handler) override {
        ASSERT(_writeRegs.find(fd) == _writeRegs.end(), );

        struct epoll_event event;
        std::memset(&event, 0, sizeof event);
        event.data.fd = fd;

        if (_readRegs.find(fd) == _readRegs.end()) {
            event.events = EPOLLOUT;
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_ADD, fd, &event), "");
        }
        else {
            event.events = EPOLLIN | EPOLLOUT;
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event), "");
        }

        _writeRegs.insert({fd, handler});
    }

    void removeWriteable(int fd) override {
        auto iter = _writeRegs.find(fd);
        ASSERT(iter != _writeRegs.end(), );

        if (_readRegs.find(fd) == _readRegs.end()) {
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_DEL, fd, nullptr), "");
        }
        else {
            struct epoll_event event;
            std::memset(&event, 0, sizeof event);
            event.events = EPOLLIN;
            event.data.fd = fd;
            THROW_IF_SYSCALL_FAILS(::epoll_ctl(_fd, EPOLL_CTL_MOD, fd, &event), "");
        }

        _writeRegs.erase(iter);
    }

    void addTimeoutable(I_TimeoutHandler * handler, int milliseconds) override {
#if DEBUG
        for (auto & reg : _timeoutRegs) {
            ASSERT(reg.handler != handler, << "Handler already registered.");
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

        _timeoutRegs.insert(iter, TimeEntry{scheduled, handler});
    }

    void removeTimeoutable(I_TimeoutHandler * handler) override {
        for (auto iter = _timeoutRegs.begin(); iter != _timeoutRegs.end(); ++iter) {
            if (iter->handler == handler) {
                _timeoutRegs.erase(iter);
                return;
            }
        }

        FATAL(<< "Handler not registered.");
    }
};

using Selector = EPollSelector;

#endif // __linux__

#endif // SUPPORT__SELECTOR__HXX
