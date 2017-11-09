// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/tty.hxx"
#include "terminol/support/time.hxx"
#include "terminol/support/sys.hxx"

#include <unistd.h>
#include <pwd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#ifdef __linux__
#include <pty.h>
#else
#include <termios.h>
#include <libutil.h>
#endif

#include <fstream>

namespace {

    // TODO consolidate this function
    std::string nthToken(const std::string & str, size_t n) {
        ASSERT(n > 0, );

        size_t i = 0;
        size_t j = 0;

        do {
            THROW_UNLESS(j != std::string::npos, ConversionError(""));
            i = str.find_first_not_of(" \t", j);
            THROW_UNLESS(i != std::string::npos, ConversionError(""));
            j = str.find_first_of(" \t", i);
            --n;
        } while (n != 0);

        if (j == std::string::npos) { j = str.size(); }
        THROW_UNLESS(i != j, ConversionError(""));

        return str.substr(i, j - i);
    }

} // namespace

Tty::Tty(I_Observer &        observer,
         I_Selector &        selector,
         const Config &      config,
         uint16_t            rows,
         uint16_t            cols,
         const std::string & windowId,
         const Command &     command)
    : _observer(observer)
    , _selector(selector)
    , _config(config)
    , _pid(0)
    , _fd(-1)
    , _dumpWrites(false)
    , _suspended(false) {
    openPty(rows, cols, windowId, command);
    ASSERT(_pid != 0, );
    ASSERT(_fd != -1, );
}

Tty::~Tty() {
    ASSERT(_pid == 0, << "Child not reaped.");

    if (_fd != -1) { close(); }
}

void Tty::tryReap() {
    ASSERT(_pid != 0, << "Child already reaped.");
    int status;
    if (pollReap(0, status)) {
        _observer.ttyReaped(status);
        _pid = 0;
    }
}

void Tty::killReap() {
    int status;

    // Give the child a chance to exit nicely.
    ::kill(_pid, SIGCONT);
    ::kill(_pid, SIGPIPE);
    if (pollReap(100, status)) { goto done; }
    ::kill(_pid, SIGINT);
    if (pollReap(100, status)) { goto done; }
    ::kill(_pid, SIGTERM);
    if (pollReap(100, status)) { goto done; }
    ::kill(_pid, SIGQUIT);
    if (pollReap(100, status)) { goto done; }
    // Too slow - knock it on the head.
    ::kill(_pid, SIGKILL);
    status = waitReap();

done:
    _observer.ttyReaped(status);
}

void Tty::resize(uint16_t rows, uint16_t cols) {
    if (_fd == -1) {
        // I've not seen this, but perhaps it is possible. Leaving it here
        // out of paranoia.
        return;
    }

    ASSERT(_fd != -1, << "PTY already closed.");
    ASSERT(_pid != 0, << "Child already reaped.");
    const struct winsize winsize = {rows, cols, 0, 0};
    THROW_IF_SYSCALL_FAILS(::ioctl(_fd, TIOCSWINSZ, &winsize), "");
}

void Tty::write(const uint8_t * data, size_t size) {
    ASSERT(!_suspended, );

    if (_dumpWrites) { return; }

    if (_fd == -1) {
        // This can happen if we read EOF but the user inputs data before
        // we get the SIGCHLD.
        return;
    }

    ASSERT(size != 0, );

    while (size != 0) {
        auto rval = TEMP_FAILURE_RETRY(::write(_fd, static_cast<const void *>(data), size));

        if (rval == -1) {
            switch (errno) {
            case EAGAIN: WARNING("Dropping: " << size << " bytes"); goto done;
            case EIO:
                // Don't close the PTY, wait for handleRead() to error.
                _dumpWrites = true;
                goto done;
            default: THROW_SYSTEM_ERROR(errno, "write()");
            }
        }
        else if (rval == 0) {
            FATAL(<< "Zero length write");
        }
        else {
            data += rval;
            size -= rval;
        }
    }

done
    : // FIXME write a utility function for read()/write()
      ;
}

bool Tty::hasSubprocess() const {
    std::ostringstream ost;
    ost << "/proc/" << _pid << "/stat";
    std::ifstream ifs(ost.str().c_str());
    if (!ifs.good()) { return false; }
    std::string line;
    if (!getline(ifs, line).good()) { return false; }
    try {
        auto pid = unstringify<pid_t>(nthToken(line, 8));
        return pid != _pid;
    }
    catch (const ConversionError & error) {
        ERROR(<< error.what());
        return false;
    }
}

void Tty::suspend() {
    ASSERT(_fd != -1, );
    ASSERT(!_suspended, );

    _selector.removeReadable(_fd);
    _suspended = true;
}

void Tty::resume() {
    ASSERT(_fd != -1, );
    ASSERT(_suspended, );

    _selector.addReadable(_fd, this);
    _suspended = false;
}

void Tty::close() {
    ASSERT(_fd != -1, );

    if (!_suspended) { _selector.removeReadable(_fd); }

    THROW_IF_SYSCALL_FAILS(::close(_fd), "");
    _fd = -1;
}

void Tty::openPty(uint16_t            rows,
                  uint16_t            cols,
                  const std::string & windowId,
                  const Command &     command) {
    ASSERT(_fd == -1, );

    int            master, slave;
    struct winsize winsize = {rows, cols, 0, 0};

    THROW_IF_SYSCALL_FAILS(::openpty(&master, &slave, nullptr, nullptr, &winsize),
                           "openpty() failed");
    ScopeGuard guard([&]() {
        TEMP_FAILURE_RETRY(::close(master));
        TEMP_FAILURE_RETRY(::close(slave));
    });

    _pid = THROW_IF_SYSCALL_FAILS(::fork(), "fork() failed");

    guard.dismiss();

    if (_pid != 0) {
        // Parent code-path.

        THROW_IF_SYSCALL_FAILS(::close(slave), "");

        // Set non-blocking.
        fdNonBlock(master);

        // Stash the master descriptor.
        _fd = master;

        _selector.addReadable(_fd, this);
    }
    else {
        // Child code-path.

        // Create a new process group.
        THROW_IF_SYSCALL_FAILS(::setsid(), "");

        // Hook stdin/out/err up to the PTY.
        THROW_IF_SYSCALL_FAILS(::close(STDIN_FILENO), "");
        THROW_IF_SYSCALL_FAILS(::fcntl(slave, F_DUPFD, STDIN_FILENO), "");
        THROW_IF_SYSCALL_FAILS(::close(STDOUT_FILENO), "");
        THROW_IF_SYSCALL_FAILS(::fcntl(slave, F_DUPFD, STDOUT_FILENO), "");
        THROW_IF_SYSCALL_FAILS(::close(STDERR_FILENO), "");
        THROW_IF_SYSCALL_FAILS(::fcntl(slave, F_DUPFD, STDERR_FILENO), "");

        // Acquire controlling TTY.
        THROW_IF_SYSCALL_FAILS(::ioctl(slave, TIOCSCTTY, nullptr), "");

        THROW_IF_SYSCALL_FAILS(::close(slave), "");
        THROW_IF_SYSCALL_FAILS(::close(master), "");

        execShell(windowId, command);
    }
}

void Tty::execShell(const std::string & windowId, const Command & command) {
    ::unsetenv("COLUMNS");
    ::unsetenv("LINES");
    ::unsetenv("TERMCAP");

    auto passwd = static_cast<const struct passwd *>(::getpwuid(::getuid()));
    if (passwd) {
        ::setenv("LOGNAME", passwd->pw_name, 1);
        ::setenv("USER", passwd->pw_name, 1);
        ::setenv("SHELL", passwd->pw_shell, 0);
        ::setenv("HOME", passwd->pw_dir, 0);
    }

    ::setenv("WINDOWID", windowId.c_str(), 1);
    ::setenv("TERM", _config.termName.c_str(), 1);
    ::setenv("XTERM_256_COLORS", "1", 1);

    ::signal(SIGCHLD, SIG_DFL);
    ::signal(SIGHUP, SIG_DFL);
    ::signal(SIGINT, SIG_DFL);
    ::signal(SIGQUIT, SIG_DFL);
    ::signal(SIGTERM, SIG_DFL);
    ::signal(SIGALRM, SIG_DFL);

    std::vector<const char *> args;

    if (command.empty()) {
        auto shell = static_cast<const char *>(::getenv("SHELL"));
        if (!shell) {
            shell = "/bin/sh";
            WARNING("Could not determine shell, falling back to: " << shell);
        }
        args.push_back(shell);
        args.push_back("-i");
    }
    else {
        for (auto & a : command) { args.push_back(a.c_str()); }
    }

    args.push_back(nullptr);

    ::execvp(args[0], const_cast<char * const *>(args.data()));

    // If we are here then the exec call failed.
    std::exit(127); // Same as ::system() for failed commands.
}

bool Tty::pollReap(int msec, int & status) {
    ASSERT(_pid != 0, );
    ASSERT(msec >= 0, );

    for (;;) {
        int   stat;
        pid_t pid;
        pid = THROW_IF_SYSCALL_FAILS(::waitpid(_pid, &stat, WNOHANG), "waitpid()");
        if (pid != 0) {
            ASSERT(pid == _pid, );
            _pid   = 0;
            status = WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
            return true;
        }

        if (msec == 0) { break; }

        --msec;
        ::usleep(1000); // 1ms
    }

    return false;
}

int Tty::waitReap() {
    ASSERT(_pid != 0, );

    int   stat;
    pid_t pid;
    pid = THROW_IF_SYSCALL_FAILS(::waitpid(_pid, &stat, 0), "waitpid()");
    ASSERT(pid == _pid, );
    _pid = 0;
    return WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
}

// I_Selector::I_ReadHandler implementation:

void Tty::handleRead(int fd) {
    if (_fd == -1) {
        // I've not seen this, but perhaps it is possible. Leaving it here
        // out of paranoia.
        return;
    }

    ASSERT(_fd != -1, );
    ASSERT(_fd == fd, );

    // It's possible to be invoked to handle read even though we are suspended
    // if the suspend() call came during a Selector::animate() when we were
    // already in the event queue. Just return now to honour our contract
    // that we are suspended.
    if (_suspended) { return; }

    Timer   timer(1000 / _config.framesPerSecond);
    uint8_t buf[BUFSIZ]; // 8192 last time I looked.
    auto    size = _config.syncTty ? 1 : sizeof buf;

    do {
        auto rval = TEMP_FAILURE_RETRY(::read(_fd, static_cast<void *>(buf), size));

        if (rval == -1) {
            switch (errno) {
            case EAGAIN:
                // Our non-blocking fd has no more data.
                goto done;
            case EIO:
                // The other end of the PTY is gone.
                close();
                goto done;
            default: THROW_SYSTEM_ERROR(errno, "read()");
            }
        }
        else if (rval == 0) {
            // The other end of the PTY is gone.
            close();
            goto done;
        }
        else {
            _observer.ttyData(buf, rval);
            if (_config.syncTty) { _observer.ttySync(); }
        }
    } while (!timer.expired());

done:
    _observer.ttySync();
}
