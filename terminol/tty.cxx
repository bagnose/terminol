// vi:noai:sw=4

#include "terminol/tty.hxx"

#include <sstream>

#include <unistd.h>
#include <pty.h>
#include <pwd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/types.h>

namespace {

template <typename T>
std::string strArgs(const std::vector<T> & args) {
    std::ostringstream str;
    bool first = true;
    for (auto a : args) {
        if (first) { first = false; }
        else       { str << ","; }
        str << a;
    }
    return str.str();
}

template <typename T>
T nthArg(const std::vector<T> & args, size_t n, const T & fallback = T()) {
    if (n < args.size()) {
        return args[n];
    }
    else {
        return fallback;
    }
}

} // namespace {anonymous}

Tty::Tty(IObserver         & observer,
         uint16_t            rows,
         uint16_t            cols,
         const std::string & windowId,
         const std::string & term,
         const Command     & command) :
    _observer(observer),
    _dispatch(false),
    _fd(-1),
    _pid(0),
    _dumpWrites(false),
    _state(STATE_NORMAL)
{
    openPty(rows, cols, windowId, term, command);
}


Tty::~Tty() {
    ASSERT(!_dispatch,);
    if (isOpen()) {
        close();
    }
}

bool Tty::isOpen() const {
    ASSERT(!_dispatch,);
    return _fd != -1;
}

int Tty::getFd() {
    ASSERT(!_dispatch,);
    ASSERT(isOpen(), "Not open.");
    return _fd;
}

void Tty::read() {
    ASSERT(!_dispatch,);
    ASSERT(isOpen(), "Not open.");
    char buffer[4096];

    ssize_t rval = ::read(_fd, static_cast<void *>(buffer), sizeof buffer);
    //PRINT("::read()=" << rval);

    if (rval == -1) {
        _observer.ttyChildExited(close());

    }
    else if (rval == 0) {
        ASSERT(false, "Expected -1 from ::read(), not EOF for child termination.");
    }
    else {
        ASSERT(rval > 0,);
        auto oldSize = _readBuffer.size();
        _readBuffer.resize(oldSize + rval);
        std::copy(buffer, buffer + rval, &_readBuffer[oldSize]);

        _dispatch = true;
        processBuffer();
        _dispatch = false;
    }
}

void Tty::enqueueWrite(const char * data, size_t size) {
    ASSERT(!_dispatch,);
    ASSERT(isOpen(), "Not open.");

    if (!_dumpWrites) {
        auto oldSize = _writeBuffer.size();
        _writeBuffer.resize(oldSize + size);
        std::copy(data, data + size, &_writeBuffer[oldSize]);
    }
}

bool Tty::isWritePending() const {
    ASSERT(!_dispatch,);
    ASSERT(isOpen(), "Not open.");
    return !_writeBuffer.empty();
}

void Tty::write() {
    ASSERT(!_dispatch,);
    ASSERT(isOpen(), "Not open.");
    ASSERT(isWritePending(), "No writes queued.");
    ASSERT(!_dumpWrites, "Dump writes is set.");

    ssize_t rval = ::write(_fd, static_cast<const void *>(&_writeBuffer.front()),
                           _writeBuffer.size());
    //PRINT("::write()=" << rval);

    if (rval == -1) {
        // The child has gone. Don't write any more data.
        _dumpWrites = true;
        _writeBuffer.clear();
    }
    else if (rval == 0) {
        ASSERT(false, "::write() zero bytes!");
    }
    else {
        _writeBuffer.erase(_writeBuffer.begin(), _writeBuffer.begin() + rval);
    }
}

void Tty::resize(uint16_t rows, uint16_t cols) {
    ASSERT(!_dispatch,);
    ASSERT(isOpen(), "Not open.");

    struct winsize winsize = { rows, cols, 0, 0 };

    ENFORCE(::ioctl(_fd, TIOCSWINSZ, &winsize) != -1,);
}

void Tty::openPty(uint16_t            rows,
                  uint16_t            cols,
                  const std::string & windowId,
                  const std::string & term,
                  const Command     & command) {
    int master, slave;
    struct winsize winsize = { rows, cols, 0, 0 };

    ENFORCE_SYS(::openpty(&master, &slave, nullptr, nullptr, &winsize) != -1,);

    _pid = ::fork();
    ENFORCE_SYS(_pid != -1, "::fork() failed.");

    if (_pid != 0) {
        // Parent code-path.

        ENFORCE_SYS(::close(slave) != -1,);
        _fd  = master;
    }
    else {
        // Child code-path.

        // Create a new process group.
        ENFORCE_SYS(::setsid() != -1, "");
        // Hook stdin/out/err up to the PTY.
        ENFORCE_SYS(::dup2(slave, STDIN_FILENO)  != -1,);
        ENFORCE_SYS(::dup2(slave, STDOUT_FILENO) != -1,);
        ENFORCE_SYS(::dup2(slave, STDERR_FILENO) != -1,);
        ENFORCE_SYS(::ioctl(slave, TIOCSCTTY, nullptr) != -1,);
        ENFORCE_SYS(::close(slave) != -1, "");
        ENFORCE_SYS(::close(master) != -1,);
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
        //shell = "/bin/sh"; // XXX use sh to avoid colour, etc. (remove this line)
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
    std::exit(127); // Same as ::system() for failed commands.
}

void Tty::processBuffer() {
    ASSERT(!_readBuffer.empty(),);

    _observer.ttyBegin();

    size_t i = 0;

    while (i != _readBuffer.size()) {
        utf8::Length length = utf8::leadLength(_readBuffer[i]);

        if (_readBuffer.size() < i + length) {
            break;
        }

        processChar(&_readBuffer[i], length);

        i += length;
    }

    _readBuffer.erase(_readBuffer.begin(), _readBuffer.begin() + i);

    _observer.ttyEnd();
}

void Tty::processChar(const char * s, utf8::Length length) {
    if (length == utf8::L1) {
        char ascii = s[0];

        if (_state == STATE_STR_ESCAPE) {
            switch (ascii) {
                case '\x1B':
                    _state = STATE_ESCAPE_START_STR;
                    break;
                case '\a':      // xterm backwards compatibility
                    processStrEscape();
                    _state = STATE_NORMAL;
                    _escapeStr.seq.clear();
                    break;
                default:
                    // XXX upper limit??
                    _escapeStr.seq.push_back(ascii);
                    break;
            }
        }
        else if (_state == STATE_ESCAPE_START_STR) {
            if (ascii == '\\') {
                processStrEscape();
            }
            _state = STATE_NORMAL;
            _escapeStr.seq.clear();
        }
        else {
            if (ascii == '\x1B') {
                ASSERT(_state == STATE_NORMAL,);
                _state = STATE_ESCAPE_START;
            }
            else if (ascii < '\x20' || ascii == '\x7F') {
                ASSERT(_state == STATE_NORMAL,);
                processControl(ascii);
            }
            else {
                switch (_state) {
                    case STATE_NORMAL:
                        _observer.ttyUtf8(&ascii, utf8::L1);
                        break;
                    case STATE_ESCAPE_START:
                        processEscape(ascii);
                        break;
                    case STATE_CSI_ESCAPE:
                        _escapeCsi.seq.push_back(ascii);

                        if (ascii >= '\x40' && ascii < '\x7F') {
                            processCsiEscape();
                            _state = STATE_NORMAL;
                            _escapeCsi.seq.clear();
                        }
                        break;
                    case STATE_STR_ESCAPE:
                        FATAL("Unreachable");
                        break;
                    case STATE_ESCAPE_START_STR:
                        FATAL("Unreachable");
                        break;
                    case STATE_TEST_ESCAPE:
                        FATAL("NYI");
                        break;
                }
            }
        }
    }
    else {
        if (_state != STATE_NORMAL) {
            ERROR("Got UTF-8 whilst in state: " << _state);
        }

        _observer.ttyUtf8(s, length);
    }
}

void Tty::processControl(char c) {
    ASSERT(_state == STATE_NORMAL,);

    switch (c) {
        case '\a':      // BEL
            _observer.ttyControl(CONTROL_BEL);
            break;
        case '\t':      // HT
            _observer.ttyControl(CONTROL_HT);
            break;
        case '\b':      // BS
            _observer.ttyControl(CONTROL_BS);
            break;
        case '\r':      // CR
            _observer.ttyControl(CONTROL_CR);
            break;
        case '\f':      // FF
        case '\v':      // VT
        case '\n':      // LF
            _observer.ttyControl(CONTROL_LF);
            break;
        case '\x0E':    // SO
        case '\x0F':    // SI
            // Ignore
            break;
        case '\x1A':    // SUB
        case '\x18':    // CAN
            // TODO reset escape states - assertion prevents this
            break;
        case '\x05':    // ENQ
        case '\0':      // NUL
        case '\x11':    // DC1/XON
        case '\x13':    // DC3/XOFF
            break;
        default:
            PRINT("Ignored control char: " << int(c));
            break;
    }
}

void Tty::processEscape(char c) {
    ASSERT(_state == STATE_ESCAPE_START,);

    switch (c) {
        case '[':
            // CSI
            _state = STATE_CSI_ESCAPE;
            break;
        case '#':
            // test
            _state = STATE_TEST_ESCAPE;
            break;
        case 'P':   // DCS
        case '_':   // APC -- Application Program Command
        case '^':   // PM  -- Privacy Message
        case ']':   // OSC -- Operating System Command
        case 'k':   // old title set compatibility
            _escapeStr.type = c;
            _state = STATE_STR_ESCAPE;
            break;
        case '(':   // Set primary charset G0
            //_altCharSet = true;           XXX
            break;
        case ')':   // Set secondary charset G1
        case '*':   // Set secondary charset G1
        case '+':   // Set secondary charset G1
            _state = STATE_NORMAL;
            break;
        case 'D':   // IND - linefeed
            FATAL("NYI st.c:2169");
            _state = STATE_NORMAL;
            break;
        case 'E':   // NEL - next line
            FATAL("NYI st.c:2177");
            _state = STATE_NORMAL;
            break;
        case 'H':   // HTS - Horizontal tab stop.
            _observer.ttySetTabStop();
            _state = STATE_NORMAL;
            break;
        case 'M':   // RI - Reverse index.
            FATAL("NYI st.c:2185");
            _state = STATE_NORMAL;
            break;
        case 'Z':   // DECID -- Identify Terminal
            FATAL("NYI st.c:2194");
            //ttywrite(VT102ID, sizeof(VT102ID) - 1);
            _state = STATE_NORMAL;
            break;
        case 'c':   // RIS - Reset to initial state
            _observer.ttyReset();
            _observer.ttyResetTitle();
            _state = STATE_NORMAL;
            break;
        case '=':   // DECPAM - Application keypad
            _observer.ttyEnableMode(MODE_APPKEYPAD);
            _state = STATE_NORMAL;
            break;
        case '>':   // DECPNM - Normal keypad
            _observer.ttyDisableMode(MODE_APPKEYPAD);
            _state = STATE_NORMAL;
            break;
        case '7':   // DECSC - Save Cursor
            FATAL("NYI st.c:2210");
            //tcursor(CURSOR_SAVE);
            _state = STATE_NORMAL;
            break;
        case '8':   // DECRC - Restore Cursor
            FATAL("NYI st.c:2214");
            //tcursor(CURSOR_LOAD);
            _state = STATE_NORMAL;
            break;
        case '\\': // ST -- Stop
            if (_state == STATE_STR_ESCAPE) {
                processStrEscape();
                _escapeStr.seq.clear();
            }
            _state = STATE_NORMAL;
            break;
        case 'm':
            break;
        default:
            ERROR("Unknown escape sequence: " << c);
            _state = STATE_NORMAL;
            break;
    }
}

void Tty::processEscapeStr(char c) {
    ASSERT(_state == STATE_ESCAPE_START_STR,);

    switch (c) {
        case '\\':
            processStrEscape();
            break;
        default:
            break;
    }

    _state = STATE_NORMAL;
}

void Tty::processCsiEscape() {
    ENFORCE(_state == STATE_CSI_ESCAPE,);       // XXX here or outside?
    ASSERT(!_escapeCsi.seq.empty(),);
    PRINT("CSI-esc: " << _escapeCsi.seq);

    size_t i = 0;
    bool priv = false;
    std::vector<int32_t> args;

    //
    // Parse the arguments.
    //

    if (_escapeCsi.seq.front() == '?') {
        ++i;
        priv = true;
    }

    bool inArg = false;

    while (i != _escapeCsi.seq.size()) {
        char c = _escapeCsi.seq[i];

        if (c >= '0' && c <= '9') {
            if (!inArg) {
                args.push_back(0);
                inArg = true;
            }
            args.back() = 10 * args.back() + c - '0';
        }
        else {
            if (inArg) {
                inArg = false;
            }

            if (c != ';') {
                break;
            }
        }

        ++i;
    }

    //
    // Handle the sequence.
    //

    if (i == _escapeCsi.seq.size()) {
        ERROR("Bad CSI: " << _escapeCsi.seq);
    }
    else {
        char mode = _escapeCsi.seq[i];
        switch (mode) {
            case 'h':
                //PRINT(<<"CSI: Set terminal mode: " << strArgs(args));
                processMode(priv, true, args);
                break;
            case 'l':
                //PRINT(<<"CSI: Reset terminal mode: " << strArgs(args));
                processMode(priv, false, args);
                break;
            case 'K':   // EL - Clear line
                switch (nthArg(args, 0)) {
                    case 0: // right
                        _observer.ttyClearLine(CLEAR_LINE_RIGHT);
                        break;
                    case 1: // left
                        _observer.ttyClearLine(CLEAR_LINE_LEFT);
                        break;
                    case 2: // all
                        _observer.ttyClearLine(CLEAR_LINE_ALL);
                        break;
                }
                break;
            case 'g':
                PRINT(<<"CSI: Tabulation clear");
                break;
            case 'H':
            case 'f': {
                uint16_t row = nthArg(args, 0, 1) - 1;
                uint16_t col = nthArg(args, 1, 1) - 1;
                //PRINT("CSI: Move cursor: row=" << row << ", col=" << col);
                _observer.ttyMoveCursor(row, col);
            }
                break;
            //case '!':
                //break;
            case 'J':
                // Clear screen.
                switch (nthArg(args, 0)) {
                    case 0:
                        // below
                        _observer.ttyClearScreen(CLEAR_SCREEN_BELOW);
                        break;
                    case 1:
                        // above
                        _observer.ttyClearScreen(CLEAR_SCREEN_ABOVE);
                        break;
                    case 2:
                        // all
                        _observer.ttyClearScreen(CLEAR_SCREEN_ALL);
                        break;
                    default:
                        FATAL("");
                }
                break;
            case 'm':
                processAttributes(args);
                break;
            default:
                PRINT(<<"CSI: UNKNOWN: mode=" << mode << ", priv=" << priv << ", args: " << strArgs(args));
                break;
        }
    }
}

void Tty::processStrEscape() {
    ENFORCE(_state == STATE_STR_ESCAPE,);       // XXX here or outside?
    PRINT("STR-esc: type=" << _escapeStr.type << ", seq=" << _escapeStr.seq);

    std::vector<std::string> args;

    //
    // Parse the arguments.
    //

    bool next = true;
    for (auto c : _escapeStr.seq) {
        if (next) { args.push_back(std::string()); next = false; }

        if (c == ';') { next = true; }
        else          { args.back().push_back(c); }
    }

    //
    // Handle the sequence.
    //

    switch (_escapeStr.type) {
        case ']':   // OSC - Operating System Command
            switch (unstringify<int>(nthArg(args, 0))) {
                case 0:
                case 1:
                case 2:
                    if (args.size() > 1) {
                        _observer.ttySetTitle(nthArg(args, 1));
                    }
                    break;
                default:
                    PRINT("Unandled: " << _escapeStr.seq);
            }
            break;
        case 'k':
            _observer.ttySetTitle(nthArg(args, 0));
            break;
        case 'P':   // DSC - Device Control String
        case '_':   // APC - Application Program Command
        case '^':   // PM Privacy Message
        default:
            PRINT("Unandled: " << _escapeStr.seq);
            break;
    }
}

void Tty::processAttributes(const std::vector<int32_t> & args) {
    for (size_t i = 0; i != args.size(); ++i) {
        int32_t v = args[i];

        switch (v) {
            case 0:
                _observer.ttySetBg(defaultBg());
                _observer.ttySetFg(defaultFg());
                _observer.ttyClearAttributes();
                break;
            case 1:
                _observer.ttyEnableAttribute(ATTRIBUTE_BOLD);
                break;
            case 3:
                _observer.ttyEnableAttribute(ATTRIBUTE_ITALIC);
                break;
            case 4:
                _observer.ttyEnableAttribute(ATTRIBUTE_UNDERLINE);
                break;
            case 5: // slow blink
            case 6: // rapid blink
                _observer.ttyEnableAttribute(ATTRIBUTE_BLINK);
                break;
            case 7:
                _observer.ttyEnableAttribute(ATTRIBUTE_REVERSE);
                break;
            case 21:
            case 22:
                _observer.ttyDisableAttribute(ATTRIBUTE_BOLD);
                break;
            case 23:
                _observer.ttyDisableAttribute(ATTRIBUTE_ITALIC);
                break;
            case 24:
                _observer.ttyDisableAttribute(ATTRIBUTE_UNDERLINE);
                break;
            case 25:
            case 26:
                _observer.ttyDisableAttribute(ATTRIBUTE_BLINK);
                break;
            case 27:
                _observer.ttyDisableAttribute(ATTRIBUTE_REVERSE);
                break;
            case 38:
                if (i + 2 < args.size() && args[i + 1] == 5) {
                    i += 2;
                    int32_t v2 = args[i];
                    if (v2 >= 0 && v2 < 256) {
                        _observer.ttySetFg(v2);
                    }
                    else {
                        ERROR("Colour out of range: " << v2);
                    }
                }
                else {
                    ERROR("XXX");
                }
                break;
            case 39:
                _observer.ttySetFg(defaultFg());
                break;
            case 48:
                if (i + 2 < args.size() && args[i + 1] == 5) {
                    i += 2;
                    int32_t v2 = args[i];
                    if (v2 >= 0 && v2 < 256) {
                        _observer.ttySetBg(v2);
                    }
                    else {
                        ERROR("Colour out of range: " << v2);
                    }
                }
                else {
                    ERROR("XXX");
                }
                break;
            case 49:
                _observer.ttySetBg(defaultBg());
                break;
            default:
                if (v >= 30 && v < 38) {
                    // normal fg
                    _observer.ttySetFg(v - 30);
                }
                else if (v >= 40 && v < 48) {
                    // bright fg
                    _observer.ttySetBg(v - 40);
                }
                else if (v >= 90 && v < 98) {
                    // normal bg
                    _observer.ttySetFg(v - 90 + 8);
                }
                else if (v >= 100 && v < 108) {
                    // bright bg
                    _observer.ttySetBg(v - 100 + 8);
                }
                else {
                    ERROR("Unhandled Blah");
                }
        }
    }
}

void Tty::processMode(bool priv, bool set, const std::vector<int32_t> & args) {
    PRINT("NYI: processMode: priv=" << priv << ", set=" <<
          set << ", args=" << strArgs(args));
}

bool Tty::pollReap(int & exitCode, int msec) {
    ASSERT(_pid != 0,);

    for (int i = 0; i != msec; ++i) {
        int stat;
        int rval = ::waitpid(_pid, &stat, WNOHANG);
        ENFORCE_SYS(rval != -1, "::waitpid() failed.");
        if (rval != 0) {
            ENFORCE(rval == _pid,);
            _pid = 0;
            exitCode = WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
            return true;
        }
        ::usleep(1000);     // 1ms
    }

    return false;
}

void Tty::waitReap(int & exitCode) {
    ASSERT(_pid != 0,);

    int stat;
    ENFORCE_SYS(::waitpid(_pid, &stat, 0) == _pid,);
    _pid = 0;
    exitCode = WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
}

int Tty::close() {
    ASSERT(isOpen(),);

    ENFORCE_SYS(::close(_fd) != -1,);
    _fd = -1;

    ::kill(_pid, SIGCONT);
    ::kill(_pid, SIGPIPE);

    int exitCode;
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
    ::kill(_pid, SIGKILL);
    waitReap(exitCode);
}
