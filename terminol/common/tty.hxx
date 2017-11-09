// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__TTY__H
#define COMMON__TTY__H

#include "terminol/common/config.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pattern.hxx"

#include <vector>
#include <string>

class Tty final
    : private Uncopyable
    , private I_Selector::I_ReadHandler {
public:
    class I_Observer {
    public:
        virtual void ttyData(const uint8_t * data, size_t size) = 0;
        virtual void ttySync()                                  = 0;
        virtual void ttyReaped(int status)                      = 0;

    protected:
        ~I_Observer() = default;
    };

private:
    I_Observer &   _observer;
    I_Selector &   _selector;
    const Config & _config;
    pid_t          _pid;
    int            _fd;
    bool           _dumpWrites;
    bool           _suspended;

public:
    using Command = std::vector<std::string>; // XXX questionable alias

    Tty(I_Observer &        observer,
        I_Selector &        selector,
        const Config &      config,
        uint16_t            rows,
        uint16_t            cols,
        const std::string & windowId,
        const Command &     command);

    ~Tty();

    void tryReap();
    void killReap();
    void resize(uint16_t rows, uint16_t cols);
    void write(const uint8_t * buffer, size_t size);
    bool hasSubprocess() const;

    void suspend();
    void resume();

private:
    void close();

    void
         openPty(uint16_t rows, uint16_t cols, const std::string & windowId, const Command & command);
    void execShell(const std::string & windowId, const Command & command);

    bool pollReap(int msec, int & status);
    int  waitReap();

    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) override;
};

#endif // COMMON__TTY__H
