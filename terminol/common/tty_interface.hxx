// vi:noai:sw=4

#ifndef COMMON__TTY_INTERFACE__H
#define COMMON__TTY_INTERFACE__H

#include <cstddef>

class I_Tty {
public:
    struct Exited {
        explicit Exited(int exitCode_) : exitCode(exitCode_) {}
        int exitCode;
    };

    struct Error {};

    // The following functions return 0 if they would block.
    virtual size_t read(char * buffer, size_t length) throw (Exited) = 0;
    virtual size_t write(const char * buffer, size_t length) throw (Error) = 0;

protected:
    ~I_Tty() {}
};

#endif // COMMON__TTY_INTERFACE__H
