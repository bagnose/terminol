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

    virtual size_t read(char * buffer, size_t length) throw (Exited) = 0;
    virtual size_t write(const char * buffer, size_t length) throw (Exited) = 0;

    //virtual ssize_t read(char * buf, size_t len) = 0;
    //virtual ssize_t write(const char * buf, size_t len) = 0;

    //virtual bool    isOpen() const = 0;

    /*
    virtual void enqueueWrite(const char * buf, size_t len) = 0;
    virtual bool isWritePending() const = 0;
    virtual void write() = 0;
    */

protected:
    I_Tty() {}
    virtual ~I_Tty() {}
};

#endif // TTY_INTERFACE__H
