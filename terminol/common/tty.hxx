// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__TTY__H
#define COMMON__TTY__H

#include "terminol/common/config.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pattern.hxx"

#include <vector>
#include <string>

class Tty :
    protected I_Selector::I_ReadHandler,
    protected Uncopyable
{
public:
    class I_Observer {
    public:
        virtual void ttyData(const uint8_t * data, size_t size) throw () = 0;
        virtual void ttySync() throw () = 0;
        virtual void ttyReaped(int status) throw () = 0;

    protected:
        ~I_Observer() {}
    };

private:
    I_Observer           & _observer;
    I_Selector           & _selector;
    const Config         & _config;
    pid_t                  _pid;
    int                    _fd;
    bool                   _dumpWrites;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    typedef std::vector<std::string> Command;           // XXX questionable typedef

    Tty(I_Observer        & observer,
        I_Selector        & selector,
        const Config      & config,
        uint16_t            rows,
        uint16_t            cols,
        const std::string & windowId,
        const Command     & command) throw (Error);

    virtual ~Tty();

    void tryReap();
    void killReap();
    void resize(uint16_t rows, uint16_t cols);
    void write(const uint8_t * buffer, size_t size);
    bool hasSubprocess() const;

protected:
    void close();

    void openPty(uint16_t            rows,
                 uint16_t            cols,
                 const std::string & windowId,
                 const Command     & command) throw (Error);
    void execShell(const std::string & windowId,
                   const Command     & command);

    bool pollReap(int msec, int & status);
    int  waitReap();

    // I_Selector::I_ReadHandler implementation:

    void handleRead(int fd) throw () override;
};

#endif // COMMON__TTY__H
