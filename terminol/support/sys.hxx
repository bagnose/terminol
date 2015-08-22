// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__SYS__HXX
#define SUPPORT__SYS__HXX

// Set FD_CLOEXEC
void fdCloseExec(int fd) noexcept;

// Set O_NONBLOCK
void fdNonBlock(int fd) noexcept;

#endif // SUPPORT__SYS__HXX
