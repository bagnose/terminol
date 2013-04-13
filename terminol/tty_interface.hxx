// vi:noai:sw=4

#ifndef TTY_INTERFACE__H
#define TTY_INTERFACE__H

#include <cstddef>

class I_Tty {
public:
    struct Exited {
        explicit Exited(int exitCode_) : exitCode(exitCode_) {}
        int exitCode;
    };

    struct Error { };

    virtual size_t read(char * buffer, size_t length) throw (Exited) = 0;
    virtual size_t write(const char * buffer, size_t length) throw (Error) = 0;

protected:
    I_Tty() {}
    ~I_Tty() {}
};

#endif // TTY_INTERFACE__H
