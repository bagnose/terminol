// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__SYS__HXX
#define SUPPORT__SYS__HXX

// Set FD_CLOEXEC
void fdCloseExec(int fd);

// Set O_NONBLOCK
void fdNonBlock(int fd);

#endif // SUPPORT__SYS__HXX
