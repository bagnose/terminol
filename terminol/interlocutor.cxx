// vi:noai:sw=4

#include "terminol/interlocutor.hxx"
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

Interlocutor::Interlocutor(I_Observer & observer,
                           I_Tty      & tty) :
    _observer(observer),
    _dispatch(false),
    _tty(tty),
    _dumpWrites(false),
    _state(STATE_NORMAL) {}

Interlocutor::~Interlocutor() {
    ASSERT(!_dispatch, "");
}

void Interlocutor::read() {
    ASSERT(!_dispatch, "");

    try {
        char buffer[4096];
        size_t rval = _tty.read(buffer, sizeof buffer);

        auto oldSize = _readBuffer.size();
        _readBuffer.resize(oldSize + rval);
        std::copy(buffer, buffer + rval, &_readBuffer[oldSize]);

        processBuffer();
    }
    catch (I_Tty::Exited & ex) {
        _observer.interChildExited(ex.exitCode);
    }
}

void Interlocutor::enqueueWrite(const char * data, size_t size) {
    ASSERT(!_dispatch, "");

    if (!_dumpWrites) {
        auto oldSize = _writeBuffer.size();
        _writeBuffer.resize(oldSize + size);
        std::copy(data, data + size, &_writeBuffer[oldSize]);
    }
}

bool Interlocutor::isWritePending() const {
    ASSERT(!_dispatch, "");
    return !_writeBuffer.empty();
}

void Interlocutor::write() {
    ASSERT(!_dispatch, "");
    ASSERT(isWritePending(), "No writes queued.");
    ASSERT(!_dumpWrites, "Dump writes is set.");

    try {
        size_t rval = _tty.write(&_writeBuffer.front(), _writeBuffer.size());
        _writeBuffer.erase(_writeBuffer.begin(), _writeBuffer.begin() + rval);
    }
    catch (I_Tty::Error & ex) {
        _dumpWrites = true;
        _writeBuffer.clear();
    }
}

void Interlocutor::processBuffer() {
    ASSERT(!_readBuffer.empty(), "");

    _dispatch = true;
    _observer.interBegin();

    size_t i = 0;

    size_t utf8Index = 0;
    size_t utf8Count = 0;
    bool   utf8Accum = false;

    while (i != _readBuffer.size()) {
        utf8::Length length = utf8::leadLength(_readBuffer[i]);

        if (_readBuffer.size() < i + length) {
            // Incomplete UTF-8 sequence.
            break;
        }

        if (_state == STATE_NORMAL && !isControl(_readBuffer[i])) {
            if (!utf8Accum) {
                utf8Index = i;
                utf8Count = 0;
                utf8Accum = true;
            }
            ++utf8Count;
        }
        else {
            if (utf8Accum) {
                _observer.interUtf8(&_readBuffer[utf8Index], utf8Count, i - utf8Index);
                utf8Accum = false;
            }

            processChar(&_readBuffer[i], length);
        }

        i += length;
    }

    if (utf8Accum) {
        _observer.interUtf8(&_readBuffer[utf8Index], utf8Count, i - utf8Index);
    }

    _readBuffer.erase(_readBuffer.begin(), _readBuffer.begin() + i);

    _observer.interEnd();
    _dispatch = false;
}

void Interlocutor::processChar(const char * s, utf8::Length length) {
    // Can be:
    // 7-bit:
    //  - control char
    //  - non-control char
    // 8-bit:
    //  - utf8

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
                        FATAL("Unreachable now");
                        //_observer.interUtf8(&ascii, utf8::L1);
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

        FATAL("Unreachable now.");
        //_observer.interUtf8(s, length);
    }
}

void Interlocutor::processControl(char c) {
    ASSERT(_state == STATE_NORMAL, "");

    switch (c) {
        case BEL:
            _observer.interControl(CONTROL_BEL);
            break;
        case HT:
            _observer.interControl(CONTROL_HT);
            break;
        case BS:
            _observer.interControl(CONTROL_BS);
            break;
        case CR:
            _observer.interControl(CONTROL_CR);
            break;
        case FF:
            _observer.interControl(CONTROL_FF);
            break;
        case VT:
            _observer.interControl(CONTROL_VT);
            break;
        case LF:
            _observer.interControl(CONTROL_LF);
            break;
        case SO:
            _observer.interControl(CONTROL_SO);
            break;
        case SI:
            _observer.interControl(CONTROL_SI);
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

void Interlocutor::processEscape(char c) {
    ASSERT(_state == STATE_ESCAPE_START, "");

    switch (c) {
        case '[':
            // CSI
            _state = STATE_CSI_ESCAPE;
            break;
        case '#':
            // test
            NYI("test escape");
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
        case '*':   // Set secondary charset G2
        case '+':   // Set secondary charset G3
            NYI("charset");
            _state = STATE_NORMAL;
            break;
        case 'D':   // IND - linefeed
            NYI("st.c:2169");
            _state = STATE_NORMAL;
            break;
        case 'E':   // NEL - next line
            NYI("st.c:2177");
            _state = STATE_NORMAL;
            break;
        case 'H':   // HTS - Horizontal tab stop.
            _observer.interSetTabStop();
            _state = STATE_NORMAL;
            break;
        case 'M':   // RI - Reverse index.
            NYI("st.c:2185");
            _state = STATE_NORMAL;
            break;
        case 'Z':   // DECID - Identify Terminal
            NYI("st.c:2194");
            //ttywrite(VT102ID, sizeof(VT102ID) - 1);
            _state = STATE_NORMAL;
            break;
        case 'c':   // RIS - Reset to initial state
            _observer.interResetAll();
            _state = STATE_NORMAL;
            break;
        case '=':   // DECPAM - Application keypad
            _observer.interSetMode(MODE_APPKEYPAD, true);
            _state = STATE_NORMAL;
            break;
        case '>':   // DECPNM - Normal keypad
            _observer.interSetMode(MODE_APPKEYPAD, false);
            _state = STATE_NORMAL;
            break;
        case '7':   // DECSC - Save Cursor
            NYI("st.c:2210");
            //tcursor(CURSOR_SAVE);
            _state = STATE_NORMAL;
            break;
        case '8':   // DECRC - Restore Cursor
            NYI("st.c:2214");
            //tcursor(CURSOR_LOAD);
            _state = STATE_NORMAL;
            break;
        //case 'm':
            //break;
        default:
            ERROR("Unknown escape sequence: " << c);
            _state = STATE_NORMAL;
            break;
    }
}

void Interlocutor::processCsiEscape() {
    ENFORCE(_state == STATE_CSI_ESCAPE, "");       // XXX here or outside?
    ASSERT(!_escapeCsi.seq.empty(), "");
    dumpCsiEscape();

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

    ASSERT(i == _escapeCsi.seq.size() - 1, "Seq: " << _escapeCsi.seq);        // XXX replaces following conditional

    if (i == _escapeCsi.seq.size()) {
        ERROR("Bad CSI: " << _escapeCsi.seq);
    }
    else {
        char mode = _escapeCsi.seq[i];
        switch (mode) {
            case '@': // ICH - Insert Character
                _observer.interInsertChars(nthArg(args, 0, 1));
                break;
            case 'A': // CUU - Cursor Up
                _observer.interRelMoveCursor(-nthArg(args, 0, 1) , 0);
                break;
            case 'B': // CUD - Cursor Down
                _observer.interRelMoveCursor(nthArg(args, 0, 1) , 0);
                break;
            case 'C': // CUF - Cursor Forward
                _observer.interRelMoveCursor(0, nthArg(args, 0, 1));
                break;
            case 'D': // CUB - Cursor Backward
                _observer.interRelMoveCursor(0, -nthArg(args, 0, 1));
                break;
            case 'E': // CNL - Cursor Next Line
                NYI('E');
                break;
            case 'F': // CPL - Cursor Previous Line
                NYI('F');
                break;
            case 'G': // CHA - Cursor Horizontal Absolute
                NYI('G');
                break;
            case 'f':       // HVP - Horizontal and Vertical Position
            case 'H': {     // CUP - Cursor Position
                uint16_t row = nthArg(args, 0, 1) - 1;
                uint16_t col = nthArg(args, 1, 1) - 1;
                //PRINT("CSI: Move cursor: row=" << row << ", col=" << col);
                _observer.interMoveCursor(row, col);
            }
                break;
            case 'I': // CHT - ??? Horizontal tab?
                _observer.interAdvanceTab(nthArg(args, 0, 1));
                break;
            case 'J': // ED - Erase Data
                // Clear screen.
                switch (nthArg(args, 0)) {
                    case 0: // below      FIXME or >2
                        _observer.interClearScreen(CLEAR_SCREEN_BELOW);
                        break;
                    case 1: // above
                        _observer.interClearScreen(CLEAR_SCREEN_ABOVE);
                        break;
                    case 2: // all
                        _observer.interClearScreen(CLEAR_SCREEN_ALL);
                        _observer.interMoveCursor(0, 0);
                        break;
                    default:
                        ERROR("!");
                }
                break;
            case 'K':   // EL - Erase line
                switch (nthArg(args, 0)) {
                    case 0: // right      FIXME or >2
                        _observer.interClearLine(CLEAR_LINE_RIGHT);
                        break;
                    case 1: // left
                        _observer.interClearLine(CLEAR_LINE_LEFT);
                        break;
                    case 2: // all
                        _observer.interClearLine(CLEAR_LINE_ALL);
                        break;
                    default:
                        ERROR("!");
                }
                break;
            case 'L': // IL - Insert Lines
                _observer.interInsertLines(nthArg(args, 0, 1));
                break;
            case 'M': // DL - Delete Lines
                _observer.interDeleteLines(nthArg(args, 0, 1));
                break;

            case 'P': // DCH - ??? Delete Character???
                NYI("DCH");
                break;
            case 'S': // SU - Scroll Up
                NYI("SU");
                break;
            case 'T': // SD - Scroll Down
                NYI("SD");
                break;
            case 'X': // ECH
                NYI("ECH");
                break;
            case 'Z': // CBT
                NYI("CBT");
                break;
            case '`': // HPA
                NYI("HPA");
                break;

            case 'b': // REP
                NYI("REP");
                break;
            case 'c': // Primary DA
                NYI("Primary DA");
                break;
            case 'd': // VPA
                NYI("VPA");
                break;

            case 'g': // TBC
                NYI("CSI: Tabulation clear");
                break;
            case 'h': // SM
                //PRINT("CSI: Set terminal mode: " << strArgs(args));
                processModes(priv, true, args);
                break;
            case 'l': // RM
                //PRINT("CSI: Reset terminal mode: " << strArgs(args));
                processModes(priv, false, args);
                break;
            case 'm': // SGR - Select Graphic Rendition
                processAttributes(args);
                break;
            case 'n': // DSR - Device Status Report
                if (args.empty()) {
                    // QDC - Query Device Code
                    // RDC - Report Device Code: <ESC>[{code}0c
                    NYI("What code should I send?");
                }
                else if (nthArg(args, 0) == 5) {
                    // QDS - Query Device Status
                    // RDO - Report Device OK: <ESC>[0n
                    std::ostringstream ost;
                    ost << ESC << "[0n";
                    std::string str = ost.str();
                    _writeBuffer.insert(_writeBuffer.begin(), str.begin(), str.end());
                }
                else if (nthArg(args, 0) == 6) {
                    // QCP - Query Cursor Position
                    // RCP - Report Cursor Position
                    uint16_t row, col;
                    _observer.interGetCursorPos(row, col);
                    std::ostringstream ost;
                    ost << ESC << '[' << row << ';' << col << 'R';
                    std::string str = ost.str();
                    _writeBuffer.insert(_writeBuffer.begin(), str.begin(), str.end());
                }
                else {
                    ERROR("FIXME");
                }
                break;
            case 'r': // DECSTBM - Set Top and Bottom Margins (scrolling)
                if (priv) {
                    NYI("!!");      // reset margin and cursor to top-left?
                }
                else {
                    // http://www.vt100.net/docs/vt510-rm/DECSTBM
                    uint16_t top = nthArg(args, 0, 1);
                    uint16_t bottom = nthArg(args, 1, 1);
                    if (bottom > top) {
                        _observer.interSetScrollTopBottom(top - 1, bottom - 1);
                    }
                    else {
                        _observer.interSetScrollTop(top - 1);
                    }
                    _observer.interMoveCursor(0, 0);
                }
                break;
            case 's': // restore cursor?
                NYI("Restore cursor?");
                break;
            case 't': // window ops?
                NYI("Window ops");
                break;
            case 'u': // Same as 's'??
                NYI("'u'");
                break;
                //case '!':
                //break;
            default:
                PRINT("CSI: UNKNOWN: mode=" << mode << ", priv=" << priv << ", args: " << strArgs(args) << ", seq=" << _escapeCsi.seq);
                break;
        }
    }
}

void Interlocutor::processStrEscape() {
    ENFORCE(_state == STATE_STR_ESCAPE, "");       // XXX here or outside?
    dumpStrEscape();

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
                case 0: // Icon name and window title
                case 1: // Icon label
                case 2: // Window title
                    if (args.size() > 1) {      // XXX need if? we have fallback
                        _observer.interSetTitle(nthArg(args, 1));
                    }
                    break;
                default:
                    PRINT("Unandled: " << _escapeStr.seq);
            }
            break;
        case 'k':
            _observer.interSetTitle(nthArg(args, 0));
            break;
        case 'P':   // DSC - Device Control String
        case '_':   // APC - Application Program Command
        case '^':   // PM Privacy Message
        default:
            PRINT("STR: UNKNOWN: type=" << _escapeStr.type << ", seq=" << _escapeStr.seq);
            break;
    }
}

void Interlocutor::processAttributes(const std::vector<int32_t> & args) {
    for (size_t i = 0; i != args.size(); ++i) {
        int32_t v = args[i];

        switch (v) {
            case 0: // Reset/Normal
                _observer.interResetBg();
                _observer.interResetFg();
                _observer.interClearAttributes();
                break;
            case 1: // Bold
                _observer.interSetAttribute(ATTRIBUTE_BOLD, true);
                break;
            case 2: // Faint (decreased intensity)
                NYI("Faint");
                break;
            case 3: // Italic: on
                _observer.interSetAttribute(ATTRIBUTE_ITALIC, true);
                break;
            case 4: // Underline: Single
                _observer.interSetAttribute(ATTRIBUTE_UNDERLINE, true);
                break;
            case 5: // Blink: slow
            case 6: // Blink: rapid
                _observer.interSetAttribute(ATTRIBUTE_BLINK, true);
                break;
            case 7: // Image: Negative
                _observer.interSetAttribute(ATTRIBUTE_REVERSE, true);
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
                _observer.interSetAttribute(ATTRIBUTE_BOLD, false);
                break;
            case 23: // Not italic, not Fraktur
                _observer.interSetAttribute(ATTRIBUTE_ITALIC, false);
                break;
            case 24: // Underline: None (not singly or doubly underlined)
                _observer.interSetAttribute(ATTRIBUTE_UNDERLINE, false);
                break;
            case 25: // Blink: off
            case 26: // Reserved
                _observer.interSetAttribute(ATTRIBUTE_BLINK, false);
                break;
            case 27: // Image: Positive
                _observer.interSetAttribute(ATTRIBUTE_REVERSE, false);
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
                        _observer.interSetFg(v2);
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
                _observer.interResetFg();
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
                        _observer.interSetBg(v2);
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
                _observer.interResetBg();
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
                    _observer.interSetFg(v - 30);
                }
                else if (v >= 40 && v < 48) {
                    // bright fg
                    _observer.interSetBg(v - 40);
                }
                else if (v >= 90 && v < 98) {
                    // normal bg
                    _observer.interSetFg(v - 90 + 8);
                }
                else if (v >= 100 && v < 108) {
                    // bright bg
                    _observer.interSetBg(v - 100 + 8);
                }
                // FOLLOWING STUFF UNTESTED:
                else if (v >= 256 && v < 512) {
                    _observer.interSetFg(v - 256);
                }
                else if (v >= 512 && v < 768) {
                    _observer.interSetBg(v - 512);
                }
                else {
                    ERROR("Unhandled attribute: " << v);
                }
                break;
        }
    }
}

void Interlocutor::processModes(bool priv, bool set, const std::vector<int32_t> & args) {
    //PRINT("processModes: priv=" << priv << ", set=" << set << ", args=" << strArgs(args));

    for (auto a : args) {
        if (priv) {
            switch (a) {
                case 1: // DECCKM - Cursor key
                    _observer.interSetMode(MODE_APPCURSOR, set);
                    break;
                case 2: // DECANM - ANSI/VT52
                    NYI("DECANM");
                    break;
                case 3: // DECCOLM - Column
                    if (set) {
                        // resize 132x24
                    }
                    else {
                        // resize 80x24
                    }
                    NYI("DECCOLM");
                    break;
                case 4:  // DECSCLM - Scroll (IGNORED)
                    NYI("DECSLM");
                    break;
                case 5: // DECSCNM - Reverse video
                    NYI("Reverse video");
                    /*
                       mode = term.mode;
                       MODBIT(term.mode, set, MODE_REVERSE);
                       if(mode != term.mode) {
                       redraw(REDRAW_TIMEOUT);
                       }
                       */
                    break;
                case 6: // DECOM - Origin
                    NYI("DECOM: origin");
                    /*
                       MODBIT(term.c.state, set, CURSOR_ORIGIN);
                       tmoveato(0, 0);
                       */
                    break;
                case 7: // DECAWM - Auto wrap
                    _observer.interSetMode(MODE_WRAP, set);
                    break;
                case 8:  // DECARM - Auto repeat (IGNORED)
                    NYI("DECARM");
                    break;
                case 12: // CVVIS/att610 - Start blinking cursor (IGNORED)
                    NYI("CVVIS/att610");
                    break;
                case 18: // DECPFF - Printer feed (IGNORED)
                case 19: // DECPEX - Printer extent (IGNORED)
                    NYI("DECPFF/DECPEX");
                    break;
                case 25: // DECTCEM - Text Cursor Enable Mode
                    _observer.interSetMode(MODE_HIDE, !set);
                    break;
                case 42: // DECNRCM - National characters (IGNORED)
                    NYI("Ignored: "  << a);
                    break;
                case 1000: // 1000,1002: enable xterm mouse report
                    _observer.interSetMode(MODE_MOUSEBTN,    set);
                    _observer.interSetMode(MODE_MOUSEMOTION, false);
                    break;
                case 1002:
                    _observer.interSetMode(MODE_MOUSEMOTION, set);
                    _observer.interSetMode(MODE_MOUSEBTN,    false);
                    break;
                case 1006:
                    _observer.interSetMode(MODE_MOUSESGR, set);
                    break;
                case 1034: // ssm/rrm, meta mode on/off
                    NYI("1034");
                    break;
                case 1037: // deleteSendsDel
                    NYI("1037");
                    break;
                case 1039: // altSendsEscape
                    NYI("1039");
                    break;
                case 47:    // XXX ???
                case 1047:
                    _observer.interSetMode(MODE_ALTSCREEN, set);
                    if(a != 1049) {
                        break;
                    }
                    // Deliberate fall through
                case 1048:
                    NYI("Ignored: "  << a);
                    /*
                       tcursor((set) ? CURSOR_SAVE : CURSOR_LOAD);
                       */
                    break;
                case 1049: // rmcup/smcup, alternative screen
                    NYI("1049 - alt screen");
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
                    _observer.interSetMode(MODE_KBDLOCK, set);
                    break;
                case 4:  // IRM - Insertion-replacement
                    _observer.interSetMode(MODE_INSERT, set);
                    break;
                case 12: // SRM - Send/Receive
                    _observer.interSetMode(MODE_ECHO, !set);
                    break;
                case 20: // LNM - Linefeed/new line
                    _observer.interSetMode(MODE_CRLF, set);
                    break;
                default:
                    ERROR("erresc: unknown set/reset mode: " <<  a);
                    break;
            }
        }
    }
}

void Interlocutor::dumpCsiEscape() const {
    std::cout << "ESC[";
    for (auto c : _escapeCsi.seq) {
        std::cout << Char(c);
    }
    std::cout << std::endl;
}

void Interlocutor::dumpStrEscape() const {
    std::cout << "ESC" << _escapeStr.type;
    for (auto c : _escapeStr.seq) {
        std::cout << Char(c);
    }
    std::cout << std::endl;
}

//
//
//

std::ostream & operator << (std::ostream & ost, Interlocutor::State state) {
    switch (state) {
        case Interlocutor::STATE_NORMAL:
            return ost  << "NORMAL";
        case Interlocutor::STATE_ESCAPE_START:
            return ost  << "ESCAPE_START";
        case Interlocutor::STATE_CSI_ESCAPE:
            return ost  << "CSI_ESCAPE";
        case Interlocutor::STATE_STR_ESCAPE:
            return ost  << "STR_ESCAPE";
        case Interlocutor::STATE_ESCAPE_START_STR:
            return ost  << "START_STR";
        case Interlocutor::STATE_TEST_ESCAPE:
            return ost  << "TEST_ESCAPE";
    }

    FATAL("Invalid state: " << static_cast<int>(state));
}
