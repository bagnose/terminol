// vi:noai:sw=4

#ifndef COMMON__TTY__H
#define COMMON__TTY__H

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/support.hxx"
#include "terminol/common/config.hxx"

#include <vector>
#include <string>

class Tty : public I_Tty {
    const Config & _config;
    pid_t          _pid;
    int            _fd;

public:
    typedef std::vector<std::string> Command;

    Tty(const Config      & config,
        uint16_t            rows,
        uint16_t            cols,
        const std::string & windowId,
        const Command     & command);

    virtual ~Tty();

    // Used by Window:

    int  getFd() { ASSERT(_fd != -1, ""); return _fd; } // Only perform select() on me.
    void resize(uint16_t rows, uint16_t cols);
    int  close();

    // I_Tty implementation (Used by Terminal):

    size_t read (uint8_t       * buffer, size_t length) throw (Exited);
    size_t write(const uint8_t * buffer, size_t length) throw (Error);

protected:
    void openPty(uint16_t            rows,
                 uint16_t            cols,
                 const std::string & windowId,
                 const Command     & command);
    void execShell(const std::string & windowId,
                   const Command     & command);

    bool pollReap(int & exitCode, int msec);
    int  waitReap();
};

#endif // COMMON__TTY__H
