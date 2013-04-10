// vi:noai:sw=4

#include "terminol/tty.hxx"
#include "terminol/ascii.hxx"

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
    ASSERT(!_dispatch, "");
    if (isOpen()) {
        close();
    }
}

bool Tty::isOpen() const {
    ASSERT(!_dispatch, "");
    return _fd != -1;
}

int Tty::getFd() {
    ASSERT(!_dispatch, "");
    ASSERT(isOpen(), "Not open.");
    return _fd;
}

void Tty::read() {
    ASSERT(!_dispatch, "");
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
        ASSERT(rval > 0, "");
        auto oldSize = _readBuffer.size();
        _readBuffer.resize(oldSize + rval);
        std::copy(buffer, buffer + rval, &_readBuffer[oldSize]);

        _dispatch = true;
        processBuffer();
        _dispatch = false;
    }
}

void Tty::enqueueWrite(const char * data, size_t size) {
    ASSERT(!_dispatch, "");
    ASSERT(isOpen(), "Not open.");

    if (!_dumpWrites) {
        auto oldSize = _writeBuffer.size();
        _writeBuffer.resize(oldSize + size);
        std::copy(data, data + size, &_writeBuffer[oldSize]);
    }
}

bool Tty::isWritePending() const {
    ASSERT(!_dispatch, "");
    ASSERT(isOpen(), "Not open.");
    return !_writeBuffer.empty();
}

void Tty::write() {
    ASSERT(!_dispatch, "");
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
    ASSERT(!_dispatch, "");
    ASSERT(isOpen(), "Not open.");

    struct winsize winsize = { rows, cols, 0, 0 };

    ENFORCE(::ioctl(_fd, TIOCSWINSZ, &winsize) != -1, "");
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
    ASSERT(!_readBuffer.empty(), "");

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
                case ESC:
                    _state = STATE_ESCAPE_START_STR;
                    break;
                case BEL:      // xterm backwards compatibility
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
            if (ascii == '\\') {    // ST
                processStrEscape();
            }
            else {
                // ???
            }
            _state = STATE_NORMAL;
            _escapeStr.seq.clear();
        }
        else {
            if (ascii == ESC) {
                ASSERT(_state == STATE_NORMAL, "");
                _state = STATE_ESCAPE_START;
            }
            else if (ascii < SPACE || ascii == DEL) {
                ASSERT(_state == STATE_NORMAL, "");
                // XXX Can be in the middle of an escape sequence
                // SUB/CAN could be used to terminate it.
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

                        if (ascii >= '@' && ascii < DEL) {
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
    ASSERT(_state == STATE_NORMAL, "");

    switch (c) {
        case BEL:
            _observer.ttyControl(CONTROL_BEL);
            break;
        case HT:
            _observer.ttyControl(CONTROL_HT);
            break;
        case BS:
            _observer.ttyControl(CONTROL_BS);
            break;
        case CR:
            _observer.ttyControl(CONTROL_CR);
            break;
        case FF:
        case VT:
        case LF:
            _observer.ttyControl(CONTROL_LF);
            break;
        case SO:
        case SI:
            // Ignore
            break;
        case CAN:
        case SUB:
            // XXX reset escape states - assertion prevents this
            break;
        case ENQ:
        case NUL:
        case DC1:    // DC1/XON
        case DC3:    // DC3/XOFF
            break;
        default:
            PRINT("Ignored control char: " << int(c));
            break;
    }
}

void Tty::processEscape(char c) {
    ASSERT(_state == STATE_ESCAPE_START, "");

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
        case '_':   // APC - Application Program Command
        case '^':   // PM  - Privacy Message
        case ']':   // OSC - Operating System Command
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
        case 'Z':   // DECID - Identify Terminal
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
            _observer.ttySetMode(MODE_APPKEYPAD, true);
            _state = STATE_NORMAL;
            break;
        case '>':   // DECPNM - Normal keypad
            _observer.ttySetMode(MODE_APPKEYPAD, false);
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
        case 'm':
            break;
        default:
            ERROR("Unknown escape sequence: " << c);
            _state = STATE_NORMAL;
            break;
    }
}

void Tty::processCsiEscape() {
    ENFORCE(_state == STATE_CSI_ESCAPE, "");       // XXX here or outside?
    ASSERT(!_escapeCsi.seq.empty(), "");
    //PRINT("CSI-esc: " << _escapeCsi.seq);

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

    ASSERT(i == _escapeCsi.seq.size() - 1, "");        // XXX replaces following conditional

    if (i == _escapeCsi.seq.size()) {
        ERROR("Bad CSI: " << _escapeCsi.seq);
    }
    else {
        char mode = _escapeCsi.seq[i];
        switch (mode) {
            case 'A': // CUU - Cursor Up
                _observer.ttyRelMoveCursor(-nthArg(args, 0, 1) , 0);
                break;
            case 'B': // CUD - Cursor Down
                _observer.ttyRelMoveCursor(nthArg(args, 0, 1) , 0);
                break;
            case 'C': // CUF - Cursor Forward
                _observer.ttyRelMoveCursor(0, nthArg(args, 0, 1));
                break;
            case 'D': // CUB - Cursor Backward
                _observer.ttyRelMoveCursor(0, -nthArg(args, 0, 1));
                break;
            case 'E': // CNL - Cursor Next Line
                break;
            case 'F': // CPL - Cursor Previous Line
                break;
            case 'G': // CHA - Cursor Horizontal Absolute
                break;

            case 'h':
                //PRINT("CSI: Set terminal mode: " << strArgs(args));
                processModes(priv, true, args);
                break;
            case 'l':
                //PRINT("CSI: Reset terminal mode: " << strArgs(args));
                processModes(priv, false, args);
                break;
            case 'K':   // EL - Erase line
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
                PRINT("CSI: Tabulation clear");
                break;
            case 'H':       // CUP - Cursor Position
            case 'f': {     // HVP - Horizontal and Vertical Position
                uint16_t row = nthArg(args, 0, 1) - 1;
                uint16_t col = nthArg(args, 1, 1) - 1;
                //PRINT("CSI: Move cursor: row=" << row << ", col=" << col);
                _observer.ttyMoveCursor(row, col);
            }
                break;
            //case '!':
                //break;
            case 'J': // ED - Erase Data
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
                        _observer.ttyMoveCursor(0, 0);
                        break;
                    default:
                        FATAL("");
                }
                break;
            case 'm': // SGR - Select Graphic Rendition
                processAttributes(args);
                break;
            case 'n': // DSR - Device Status Report
                if (nthArg(args, 0) == 6) {
                    uint16_t row, col;
                    _observer.ttyGetCursorPos(row, col);
                    std::ostringstream ost;
                    ost << ESC << '[' << row << ';' << col << 'R';
                    std::string str = ost.str();
                    _writeBuffer.insert(_writeBuffer.begin(), str.begin(), str.end());
                }
                else {
                    goto Default;
                }
                break;
Default:
            default:
                PRINT("CSI: UNKNOWN: mode=" << mode << ", priv=" << priv << ", args: " << strArgs(args));
                break;
        }
    }
}

void Tty::processStrEscape() {
    ENFORCE(_state == STATE_STR_ESCAPE, "");       // XXX here or outside?
    //PRINT("STR-esc: type=" << _escapeStr.type << ", seq=" << _escapeStr.seq);

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
                    if (args.size() > 1) {      // XXX need if? we have fallback
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
            case 0: // Reset/Normal
                _observer.ttySetBg(defaultBg());
                _observer.ttySetFg(defaultFg());
                _observer.ttyClearAttributes();
                break;
            case 1: // Bold
                _observer.ttySetAttribute(ATTRIBUTE_BOLD, true);
                break;
            case 2: // Faint (decreased intensity)
                NYI("Faint");
                break;
            case 3: // Italic: on
                _observer.ttySetAttribute(ATTRIBUTE_ITALIC, true);
                break;
            case 4: // Underline: Single
                _observer.ttySetAttribute(ATTRIBUTE_UNDERLINE, true);
                break;
            case 5: // Blink: slow
            case 6: // Blink: rapid
                _observer.ttySetAttribute(ATTRIBUTE_BLINK, true);
                break;
            case 7: // Image: Negative
                _observer.ttySetAttribute(ATTRIBUTE_REVERSE, true);
                break;
            case 8: // Conceal (not widely supported)
                NYI("Conceal");
                break;
            case 9: // Crossed-out (not widely supported)
                NYI("Crossed-out");
                break;
            case 10: // Primary (default) font
                NYI("Primary (default) font");
                break;
            case 11: // 1st alternative font
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19: // 9th alternative font
                NYI(nthStr(v - 10) << " alternative font");
                break;
            case 20: // Fraktur (hardly ever supported)
                NYI("Fraktur");
                break;
            case 21:
                // Bold: off or Underline: Double (bold off not widely supported,
                //                                 double underline hardly ever)
            case 22: // Normal color or intensity (neither bold nor faint)
                _observer.ttySetAttribute(ATTRIBUTE_BOLD, false);
                break;
            case 23: // Not italic, not Fraktur
                _observer.ttySetAttribute(ATTRIBUTE_ITALIC, false);
                break;
            case 24: // Underline: None (not singly or doubly underlined)
                _observer.ttySetAttribute(ATTRIBUTE_UNDERLINE, false);
                break;
            case 25: // Blink: off
            case 26: // Reserved
                _observer.ttySetAttribute(ATTRIBUTE_BLINK, false);
                break;
            case 27: // Image: Positive
                _observer.ttySetAttribute(ATTRIBUTE_REVERSE, false);
                break;
            case 28: // Reveal (conceal off - not widely supported)
                NYI("Reveal");
                break;
            case 29: // Not crossed-out (not widely supported)
                NYI("Crossed-out");
                break;
                // 30..37 (set foreground colour - handled separately)
            case 38: // Set xterm-256 text color (wikipedia: dubious???)
                if (i + 4 < args.size() && args[i + 1] == 2) {
                    // 24-bit foreground support
                    // ESC[ … 38;2;<r>;<g>;<b> … m Select RGB foreground color
                    NYI("24-bit foreground");
                }
                else if (i + 2 < args.size() && args[i + 1] == 5) {
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
                    ERROR("Unrecognised foreground attributes");
                }
                break;
            case 39:
                _observer.ttySetFg(defaultFg());
                break;
                // 40..47 (set background colour - handled separately)
            case 48:
                if (i + 4 < args.size() && args[i + 1] == 2) {
                    // 24-bit background support
                    // ESC[ … 48;2;<r>;<g>;<b> … m Select RGB foreground color
                    NYI("24-bit background");
                }
                else if (i + 2 < args.size() && args[i + 1] == 5) {
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
                    ERROR("Unrecognised background attributes");
                }
                break;
            case 49:
                _observer.ttySetBg(defaultBg());
                break;
                // 50 Reserved
            case 51: // Framed
                NYI("Framed");
                break;
            case 52: // Encircled
                NYI("Encircled");
                break;
            case 53: // Overlined
                NYI("Overlined");
                break;
            case 54: // Not framed or encircled
                NYI("Not framed or encircled");
                break;
            case 55: // Not overlined
                NYI("Not overlined");
                break;
                // 56..59 Reserved
                // 60..64 (ideogram stuff - hardly ever supported)
                // 90..99 Set foreground colour high intensity - handled separately
                // 100..109 Set background colour high intensity - handled separately
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
                    ERROR("Unhandled attribute: " << v);
                }
        }
    }
}

void Tty::processModes(bool priv, bool set, const std::vector<int32_t> & args) {
    PRINT("processModes: priv=" << priv << ", set=" <<
          set << ", args=" << strArgs(args));

    for (auto a : args) {
        if (priv) {
            switch (a) {
                case 1: // DECCKM - Cursor key
                    _observer.ttySetMode(MODE_APPCURSOR, set);
                    break;
                case 5: // DECSCNM - Reverse video
                    /*
                       mode = term.mode;
                       MODBIT(term.mode, set, MODE_REVERSE);
                       if(mode != term.mode) {
                       redraw(REDRAW_TIMEOUT);
                       }
                       */
                    break;
                case 6: // DECOM - Origin
                    /*
                       MODBIT(term.c.state, set, CURSOR_ORIGIN);
                       tmoveato(0, 0);
                       */
                    break;
                case 7: // DECAWM - Auto wrap
                    _observer.ttySetMode(MODE_WRAP, set);
                    break;
                case 0:  // Error (IGNORED) */
                case 2:  // DECANM - ANSI/VT52 (IGNORED)
                case 3:  // DECCOLM - Column  (IGNORED)
                case 4:  // DECSCLM - Scroll (IGNORED)
                case 8:  // DECARM - Auto repeat (IGNORED)
                case 18: // DECPFF - Printer feed (IGNORED)
                case 19: // DECPEX - Printer extent (IGNORED)
                case 42: // DECNRCM - National characters (IGNORED)
                case 12: // att610 - Start blinking cursor (IGNORED)
                    break;
                case 25: // DECTCEM - Text Cursor Enable Mode
                    _observer.ttySetMode(MODE_HIDE, !set);
                    break;
                case 1000: // 1000,1002: enable xterm mouse report
                    _observer.ttySetMode(MODE_MOUSEBTN,    set);
                    _observer.ttySetMode(MODE_MOUSEMOTION, false);
                    break;
                case 1002:
                    _observer.ttySetMode(MODE_MOUSEMOTION, set);
                    _observer.ttySetMode(MODE_MOUSEBTN,    false);
                    break;
                case 1006:
                    _observer.ttySetMode(MODE_MOUSESGR, set);
                    break;
                case 1049: // = 1047 and 1048 */
                case 47:
                case 1047:
                    _observer.ttySetMode(MODE_ALTSCREEN, set);
                    if(a != 1049) {
                        break;
                    }
                    // Deliberate fall through
                case 1048:
                    /*
                       tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
                       */
                    break;
                default:
                    ERROR("erresc: unknown private set/reset mode : " << a);
                    break;
            }
        }
        else {
            switch(a) {
                case 0:  // Error (IGNORED)
                    break;
                case 2:  // KAM - keyboard action
                    _observer.ttySetMode(MODE_KBDLOCK, set);
                    break;
                case 4:  // IRM - Insertion-replacement
                    _observer.ttySetMode(MODE_INSERT, set);
                    break;
                case 12: // SRM - Send/Receive
                    _observer.ttySetMode(MODE_ECHO, !set);
                    break;
                case 20: // LNM - Linefeed/new line
                    _observer.ttySetMode(MODE_CRLF, set);
                    break;
                default:
                    ERROR("erresc: unknown set/reset mode: " <<  a);
                    break;
            }
        }
    }
}

bool Tty::pollReap(int & exitCode, int msec) {
    ASSERT(_pid != 0, "");

    for (int i = 0; i != msec; ++i) {
        int stat;
        int rval = ::waitpid(_pid, &stat, WNOHANG);
        ENFORCE_SYS(rval != -1, "::waitpid() failed.");
        if (rval != 0) {
            ENFORCE(rval == _pid, "");
            _pid = 0;
            exitCode = WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
            return true;
        }
        ::usleep(1000);     // 1ms
    }

    return false;
}

void Tty::waitReap(int & exitCode) {
    ASSERT(_pid != 0, "");

    int stat;
    ENFORCE_SYS(::waitpid(_pid, &stat, 0) == _pid, "");
    _pid = 0;
    exitCode = WIFEXITED(stat) ? WEXITSTATUS(stat) : EXIT_FAILURE;
}

int Tty::close() {
    ASSERT(isOpen(), "");

    ENFORCE_SYS(::close(_fd) != -1, "");
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
