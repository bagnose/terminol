// vi:noai:sw=4

#include "terminol/common/tty.hxx"
#include "terminol/support/time.hxx"

#include <unistd.h>
#include <pty.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/fcntl.h>

#include <fstream>

namespace {

// TODO consolidate this function
std::string nthToken(const std::string & str, size_t n) throw (ParseError) {
    size_t i = 0;
    size_t j = 0;

    do {
        if (j == std::string::npos) { throw ParseError(""); }
        i = str.find_first_not_of(" \t", j);
        if (i == std::string::npos) { throw ParseError(""); }
        j = str.find_first_of(" \t", i);
        --n;
    } while (n != 0);

    if (j == std::string::npos) { j = str.size(); }
    if (i == j) { throw ParseError(""); }

    return str.substr(i, j - i);
}

} // namespace {anonymous}

Tty::Tty(I_Observer        & observer,
         I_Selector        & selector,
         const Config      & config,
         uint16_t            rows,
         uint16_t            cols,
         const std::string & windowId,
         const Command     & command) throw (Error) :
    _observer(observer),
    _selector(selector),
    _config(config),
    _pid(0),
    _fd(-1),
    _dumpWrites(false)
{
    openPty(rows, cols, windowId, command);
}

Tty::~Tty() {
    if (_fd != -1) {
        close();
    }
}

void Tty::resize(uint16_t rows, uint16_t cols) {
    ASSERT(_fd != -1, "");
    const struct winsize winsize = { rows, cols, 0, 0 };
    ENFORCE_SYS(::ioctl(_fd, TIOCSWINSZ, &winsize) != -1, "");
}

void Tty::write(const uint8_t * data, size_t size) {
    if (_dumpWrites) {
        return;
    }

    ASSERT(_fd != -1, "");
    ASSERT(size != 0, "");

    while (size != 0) {
        auto rval =
            TEMP_FAILURE_RETRY(::write(_fd, static_cast<const void *>(data), size));

        if (rval == -1) {
            switch (errno) {
                case EAGAIN:
                    WARNING("Dropping: " << size << " bytes");
                    goto done;
                case EIO:
                    _dumpWrites = true;
                    goto done;
                default:
                    FATAL("Unexpected error: " << errno << " " << ::strerror(errno));
            }
        }
        else if (rval == 0) {
            FATAL("Zero length write.");
        }
        else {
            data += rval;
            size -= rval;
        }
    }

done:                   // FIXME write a utility function for read()/write()
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
    catch (const ParseError & ex) {
        ERROR(ex.message);
        return false;
    }
}

int Tty::close() {
    ASSERT(_fd != -1, "");

    _selector.removeReadable(_fd);

    ENFORCE_SYS(::close(_fd) != -1, "::close() failed");
    _fd = -1;

    int exitCode;

    // Maybe the child has already died.
    if (pollReap(0, exitCode)) { return exitCode; }

    ::kill(_pid, SIGCONT);
    ::kill(_pid, SIGPIPE);

    // Give the child a chance to exit nicely.
    if (pollReap(100, exitCode)) { return exitCode; }
    PRINT("Sending SIGINT.");
    ::kill(_pid, SIGINT);
    if (pollReap(100, exitCode)) { return exitCode; }
    PRINT("Sending SIGTERM.");
    ::kill(_pid, SIGTERM);
    if (pollReap(100, exitCode)) { return exitCode; }
    PRINT("Sending SIGQUIT.");
    ::kill(_pid, SIGQUIT);
    if (pollReap(100, exitCode)) { return exitCode; }
    PRINT("Sending SIGKILL.");

    // Too slow - knock it on the head.
    ::kill(_pid, SIGKILL);
    return waitReap();
}

void Tty::openPty(uint16_t            rows,
                  uint16_t            cols,
                  const std::string & windowId,
                  const Command     & command) throw (Error) {
    ASSERT(_fd == -1, "");

    int master, slave;
    struct winsize winsize = { rows, cols, 0, 0 };

    if (::openpty(&master, &slave, nullptr, nullptr, &winsize) == -1) {
        throw Error("openpty() failed.");
    }

    auto guard = scopeGuard([&] { ::close(master); ::close(slave); });

    _pid = ::fork();

    if (_pid == -1) {
        throw Error("fork() failed.");
    }

    guard.dismiss();

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

        _selector.addReadable(_fd, this);
    }
    else {
        // Child code-path.

        // Create a new process group.
        ENFORCE_SYS(::setsid() != -1, "");
        // Hook stdin/out/err up to the PTY.
#if 0
        ENFORCE_SYS(::dup2(slave, STDIN_FILENO)  != -1, "");
        ENFORCE_SYS(::dup2(slave, STDOUT_FILENO) != -1, "");
        ENFORCE_SYS(::dup2(slave, STDERR_FILENO) != -1, "");
#else
        ENFORCE_SYS(::close(STDIN_FILENO) != -1, "");
        ENFORCE_SYS(::fcntl(slave, F_DUPFD, STDIN_FILENO) != -1, "");
        ENFORCE_SYS(::close(STDOUT_FILENO) != -1, "");
        ENFORCE_SYS(::fcntl(slave, F_DUPFD, STDOUT_FILENO) != -1, "");
        ENFORCE_SYS(::close(STDERR_FILENO) != -1, "");
        ENFORCE_SYS(::fcntl(slave, F_DUPFD, STDERR_FILENO) != -1, "");
#endif
        ENFORCE_SYS(::ioctl(slave, TIOCSCTTY, nullptr) != -1, "");
        ENFORCE_SYS(::close(slave) != -1, "");
        ENFORCE_SYS(::close(master) != -1, "");
        execShell(windowId, command);
    }
}

void Tty::execShell(const std::string & windowId,
                    const Command     & command) {
    ::unsetenv("COLUMNS");
    ::unsetenv("LINES");
    ::unsetenv("TERMCAP");

    auto passwd = static_cast<const struct passwd *>(::getpwuid(::getuid()));
    if (passwd) {
        ::setenv("LOGNAME", passwd->pw_name,  1);
        ::setenv("USER",    passwd->pw_name,  1);
        ::setenv("SHELL",   passwd->pw_shell, 0);
        ::setenv("HOME",    passwd->pw_dir,   0);
    }

    ::setenv("WINDOWID", windowId.c_str(), 1);
    ::setenv("TERM", _config.termName.c_str(), 1);

    ::signal(SIGCHLD, SIG_DFL);
    ::signal(SIGHUP,  SIG_DFL);
    ::signal(SIGINT,  SIG_DFL);
    ::signal(SIGQUIT, SIG_DFL);
    ::signal(SIGTERM, SIG_DFL);
    ::signal(SIGALRM, SIG_DFL);

    std::vector<const char *> args;

    if (command.empty()) {
        auto shell = static_cast<const char *>(::getenv("SHELL"));
        if (!shell) {
            WARNING("Could not determine shell, falling back to: " << shell);
            shell = "/bin/sh";
        }
        args.push_back(shell);
        args.push_back("-i");
    }
    else {
        for (auto & a : command) {
            args.push_back(a.c_str());
        }
    }

    args.push_back(nullptr);

    ::execvp(args[0], const_cast<char * const *>(&args.front()));

    // If we are here then the exec call failed.
    std::exit(127); // Same as ::system() for failed commands.
}

bool Tty::pollReap(int msec, int & exitCode) {
    ASSERT(_pid != 0, "");
    ASSERT(msec >= 0, "");

    for (;;) {
        int  stat;
        auto pid = TEMP_FAILURE_RETRY(::waitpid(_pid, &stat, WNOHANG));
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

// I_ReadHandler implementation:

void Tty::handleRead(int fd) {
    ASSERT(_fd == fd, "");

    Timer   timer(1000 / _config.framesPerSecond);
    uint8_t buf[BUFSIZ];          // 8192 last time I looked.
    auto    size = _config.syncTty ? 1 : sizeof buf;

    do {
        auto rval = TEMP_FAILURE_RETRY(::read(_fd, static_cast<void *>(buf), size));

        if (rval == -1) {
            switch (errno) {
                case EAGAIN:
                    goto done;
                case EIO:
                    _observer.ttyExited(close());
                    goto done;
                default:
                    FATAL("Unexpected error: " << errno << " " << ::strerror(errno));
            }
        }
        else if (rval == 0) {
            FATAL("Zero length read.");
        }
        else {
            _observer.ttyData(buf, rval);
            if (_config.syncTty) { _observer.ttySync(); }
        }
    } while (!timer.expired());

done:
    _observer.ttySync();
}
