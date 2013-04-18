// vi:noai:sw=4

#ifndef TTY__H
#define TTY__H

#include "terminol/common/tty_interface.hxx"

#include <vector>
#include <string>

class Tty : public I_Tty {
    pid_t _pid;
    int   _fd;
public:
    typedef std::vector<std::string> Command;

    Tty(uint16_t            rows,
        uint16_t            cols,
        const std::string & windowId,
        const std::string & term,
        const Command     & command);

    virtual ~Tty();

    int  getFd() { return _fd; }
    void resize(uint16_t rows, uint16_t cols);
    int  close();

    // I_Tty implementation:

    size_t read(char * buffer, size_t length) throw (Exited);
    size_t write(const char * buffer, size_t length) throw (Error);

protected:
    void openPty(uint16_t            rows,
                 uint16_t            cols,
                 const std::string & windowId,
                 const std::string & term,
                 const Command     & command);
    static void execShell(const std::string & windowId,
                          const std::string & term,
                          const Command     & command);

    bool pollReap(int & exitCode, int msec);
    void waitReap(int & exitCode);
};

#endif // TTY__H
