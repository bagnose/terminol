// vi:noai:sw=4

#include "terminol/common/tty.hxx"
#include "terminol/common/support.hxx"

#include <unistd.h>
#include <pty.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/fcntl.h>

Tty::Tty(uint16_t            rows,
         uint16_t            cols,
         const std::string & windowId,
         const std::string & term,
         const Command     & command) :
    _pid(0),
    _fd(-1)
{
    openPty(rows, cols, windowId, term, command);
}

Tty::~Tty() {
    if (_fd != -1) {
        close();
    }
}

void Tty::resize(uint16_t rows, uint16_t cols) {
    ASSERT(_fd != -1, "");
    struct winsize winsize = { rows, cols, 0, 0 };
    ENFORCE_SYS(::ioctl(_fd, TIOCSWINSZ, &winsize) != -1, "");
}

size_t Tty::read(char * buffer, size_t length) throw (Exited) {
    ASSERT(_fd != -1, "");
    ASSERT(length != 0, "");

    ssize_t rval = TEMP_FAILURE_RETRY(::read(_fd, static_cast<void *>(buffer), length));

    if (rval == -1) {
        switch (errno) {
            case EAGAIN:
                return 0;
            case EIO:
                throw Exited(close());
            default:
                FATAL("Unexpected error: " << errno << " " << ::strerror(errno));
        }
    }
    else if (rval == 0) {
        FATAL("Zero length read.");
    }
    else {
        return rval;
    }
}

size_t Tty::write(const char * buffer, size_t length) throw (Error) {
    ASSERT(_fd != -1, "");
    ASSERT(length != 0, "");

    ssize_t rval =
        TEMP_FAILURE_RETRY(::write(_fd, static_cast<const void *>(buffer), length));

    if (rval == -1) {
        switch (errno) {
            case EAGAIN:
                return 0;
            case EIO:       // default: ??
                throw Error();
            default:
                FATAL("Unexpected error: " << errno << " " << ::strerror(errno));
        }
    }
    else if (rval == 0) {
        FATAL("Zero length write.");
    }
    else {
        return rval;
    }
}

void Tty::openPty(uint16_t            rows,
                  uint16_t            cols,
                  const std::string & windowId,
                  const std::string & term,
                  const Command     & command) {
    int master, slave;
    struct winsize winsize = { rows, cols, 0, 0 };

    ENFORCE_SYS(::openpty(&master, &slave, nullptr, nullptr, &winsize) != -1, "");

    _pid = ::fork();
    ENFORCE_SYS(_pid != -1, "::fork() failed.");

    if (_pid != 0) {
        // Parent code-path.

        ENFORCE_SYS(::close(slave) != -1, "");

        // Set non-blocking.
        int flags;
        ENFORCE_SYS((flags = ::fcntl(master, F_GETFL)) != -1, "");
        flags |= O_NONBLOCK;
        ENFORCE_SYS(::fcntl(master, F_SETFL, flags) != -1, "");

        // Stash the master descriptor.
        _fd  = master;
    }
    else {
        // Child code-path.

        // Create a new process group.
        ENFORCE_SYS(::setsid() != -1, "");
        // Hook stdin/out/err up to the PTY.
        ENFORCE_SYS(::dup2(slave, STDIN_FILENO)  != -1, "");
        ENFORCE_SYS(::dup2(slave, STDOUT_FILENO) != -1, "");
        ENFORCE_SYS(::dup2(slave, STDERR_FILENO) != -1, "");
        ENFORCE_SYS(::ioctl(slave, TIOCSCTTY, nullptr) != -1, "");
        ENFORCE_SYS(::close(slave) != -1, "");
        ENFORCE_SYS(::close(master) != -1, "");
        execShell(windowId, term, command);
    }
}

void Tty::execShell(const std::string & windowId,
                    const std::string & term,
                    const Command     & command) {
    ::unsetenv("COLUMNS");
    ::unsetenv("LINES");
    ::unsetenv("TERMCAP");

    const struct passwd * passwd = ::getpwuid(::getuid());
    if (passwd) {
        ::setenv("LOGNAME", passwd->pw_name,  1);
        ::setenv("USER",    passwd->pw_name,  1);
        ::setenv("SHELL",   passwd->pw_shell, 0);
        ::setenv("HOME",    passwd->pw_dir,   0);
    }

    ::setenv("WINDOWID", windowId.c_str(), 1);
    ::setenv("TERM", term.c_str(), 1);

    ::signal(SIGCHLD, SIG_DFL);
    ::signal(SIGHUP,  SIG_DFL);
    ::signal(SIGINT,  SIG_DFL);
    ::signal(SIGQUIT, SIG_DFL);
    ::signal(SIGTERM, SIG_DFL);
    ::signal(SIGALRM, SIG_DFL);

    std::vector<const char *> args;

    if (command.empty()) {
        const char * shell = std::getenv("SHELL");
        if (!shell) {
            shell = "/bin/sh";
            WARNING("Could not determine shell, falling back to: " << shell);
        }
        args.push_back(shell);
        args.push_back("-i");
    }
    else {
        for (const auto & a : command) {
            args.push_back(a.c_str());
        }
    }

    args.push_back(nullptr);
    ::execvp(args[0], const_cast<char * const *>(&args.front()));
    // If we are here then the exec call failed.
    std::exit(127); // Same as ::system() for failed commands.
}

int Tty::close() {
    ASSERT(_fd != -1, "");

    ENFORCE_SYS(::close(_fd) != -1, "::close() failed");
    _fd = -1;

    int exitCode;

    // Maybe the child has already died.
    if (pollReap(exitCode, 0)) { return exitCode; }

    ::kill(_pid, SIGCONT);
    ::kill(_pid, SIGPIPE);

    // Give the child a chance to exit nicely.
    if (pollReap(exitCode, 100)) { return exitCode; }
    PRINT("Sending SIGINT.");
    ::kill(_pid, SIGINT);
    if (pollReap(exitCode, 100)) { return exitCode; }
    PRINT("Sending SIGTERM.");
    ::kill(_pid, SIGTERM);
    if (pollReap(exitCode, 100)) { return exitCode; }
    PRINT("Sending SIGQUIT.");
    ::kill(_pid, SIGQUIT);
    if (pollReap(exitCode, 100)) { return exitCode; }
    PRINT("Sending SIGKILL.");

    // Too slow - knock it on the head.
    ::kill(_pid, SIGKILL);
    return waitReap();
}

bool Tty::pollReap(int & exitCode, int msec) {
    ASSERT(_pid != 0, "");
    ASSERT(msec >= 0, "");

    for (;;) {
        int stat;
        pid_t pid = ::waitpid(_pid, &stat, WNOHANG);
        ENFORCE_SYS(pid != -1, "::waitpid() failed.");
        if (pid != 0) {
            ENFORCE(pid == _pid, "pid mismatch.");
            _pid = 0;
            exitCode = WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
            return true;
        }

        if (msec == 0) {
            break;
        }

        --msec;
        ::usleep(1000);     // 1ms
    }

    return false;
}

int Tty::waitReap() {
    ASSERT(_pid != 0, "");

    int stat;
    ENFORCE_SYS(::waitpid(_pid, &stat, 0) == _pid, "::waitpid() failed.");
    _pid = 0;
    return WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
}
