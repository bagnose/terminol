// vi:noai:sw=4

#include "terminol/common/terminal.hxx"

#include <algorithm>

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

//
//
//

Terminal::Terminal(I_Observer   & observer,
                   const KeyMap & keyMap,
                   I_Tty        & tty,
                   uint16_t       rows,
                   uint16_t       cols) :
    _observer(observer),
    _dispatch(false),
    //
    _buffer(rows, cols),
    _cursorRow(0), _cursorCol(0),
    _bg(Cell::defaultBg()),
    _fg(Cell::defaultFg()),
    _attrs(Cell::defaultAttrs()),
    _modes(),
    _tabs(_buffer.getCols()),
    //
    _keyMap(keyMap),
    _tty(tty),
    _dumpWrites(false),
    _writeBuffer(),
    _state(State::NORMAL),
    _outerState(State::NORMAL),
    _escSeq()
{
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }
    _modes.set(Mode::WRAP);
}

Terminal::~Terminal() {
    ASSERT(!_dispatch, "");
}

void Terminal::resize(uint16_t rows, uint16_t cols) {
    ASSERT(!_dispatch, "");
    ASSERT(rows > 0 && cols > 0, "");
    _cursorRow = std::min<uint16_t>(_cursorRow, rows - 1);
    _cursorCol = std::min<uint16_t>(_cursorCol, cols - 1);
    _buffer.resize(rows, cols);
    _tabs.resize(cols);
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }
}

void Terminal::keyPress(xkb_keysym_t keySym, uint8_t state) {
    std::string str;
    if (_keyMap.convert(keySym, state,
                        _modes.get(Mode::APPKEYPAD),
                        _modes.get(Mode::APPCURSOR),
                        _modes.get(Mode::CRLF),
                        str)) {
        write(str.data(), str.size());
    }
}

void Terminal::read() {
    ASSERT(!_dispatch, "");

    _dispatch = true;
    _observer.terminalBegin();

    try {
        Timer timer(50);
        char buffer[BUFSIZ];        // 8192 last time I looked.
        do {
            size_t rval = _tty.read(buffer, sizeof buffer);
            if (rval == 0) { /*PRINT("Would block");*/ break; }
            //PRINT("Read: " << rval);
            processRead(buffer, rval);
        } while (!timer.expired());
    }
    catch (I_Tty::Exited & ex) {
        _observer.terminalChildExited(ex.exitCode);
    }

    _observer.terminalEnd();
    _dispatch = false;
}

bool Terminal::needsFlush() const {
    ASSERT(!_dispatch, "");
    return !_writeBuffer.empty();
}

void Terminal::flush() {
    ASSERT(!_dispatch, "");
    ASSERT(needsFlush(), "No writes queued.");
    ASSERT(!_dumpWrites, "Dump writes is set.");

    try {
        while (!_writeBuffer.empty()) {
            size_t rval = _tty.write(&_writeBuffer.front(), _writeBuffer.size());
            if (rval == 0) { break; }
            _writeBuffer.erase(_writeBuffer.begin(), _writeBuffer.begin() + rval);
        }
    }
    catch (I_Tty::Error & ex) {
        _dumpWrites = true;
        _writeBuffer.clear();
    }
}

void Terminal::write(const char * data, size_t size) {
    ASSERT(!_dispatch, "");

    if (_dumpWrites) { return; }

    if (_writeBuffer.empty()) {
        // Try to write it now, queue what we can't write.
        try {
            while (size != 0) {
                size_t rval = _tty.write(data, size);
                if (rval == 0) { PRINT("Write would block!"); break; }
                data += rval; size -= rval;
            }

            // Copy remainder (if any) to queue
            if (size != 0) {
                auto oldSize = _writeBuffer.size();
                _writeBuffer.resize(oldSize + size);
                std::copy(data, data + size, &_writeBuffer[oldSize]);
            }
        }
        catch (I_Tty::Error &) {
            _dumpWrites = true;
            _writeBuffer.clear();
        }
    }
    else {
        // Just copy it to the queue.
        // We are assuming the write() would still block. Wait for a flush().
        auto oldSize = _writeBuffer.size();
        _writeBuffer.resize(oldSize + size);
        std::copy(data, data + size, &_writeBuffer[oldSize]);
    }
}

void Terminal::resetAll() {
    _buffer.clearAll();

    _cursorRow = 0;
    _cursorCol = 0;

    _bg    = Cell::defaultBg();
    _fg    = Cell::defaultFg();
    _attrs = Cell::defaultAttrs();

    _modes.clear();

    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }

    _observer.terminalResetTitle();
}

void Terminal::processRead(const char * data, size_t size) {
    while (size != 0) {
        switch (_utf8Machine.next(*data)) {
            case utf8::Machine::State::ACCEPT:
                processChar(_utf8Machine.seq(), _utf8Machine.length());
                break;
            case utf8::Machine::State::REJECT:
                ERROR("Rejecting UTF-8 data.");
                break;
            default:
                break;
        }

        ++data; --size;
    }
}

void Terminal::processChar(utf8::Seq seq, utf8::Length len) {
    char lead = seq.bytes[0];

    switch (_state) {
        case State::NORMAL:
            if (lead == ESC) {
                ASSERT(len == utf8::Length::L1, "");
                _state = State::ESCAPE;
                _outerState = State::NORMAL;
                ASSERT(_escSeq.empty(), "");
            }
            else if (lead >= NUL && lead < SPACE) {
                ASSERT(len == utf8::Length::L1, "");
                processControl(lead);
            }
            else {
                processNormal(seq, len);
            }
            break;
        case State::ESCAPE:
            std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            switch (lead) {
                case 'P':
                    _state = State::DCS;
                    break;
                case '[':
                    _state = State::CSI;
                    break;
                case ']':
                /////case 'k':   // old title set compatibility
                    _state = State::OSC;
                    break;
                case '#':   // Test?
                case '(':   // Set primary charset G0
                case ')':   // Set primary charset G1
                //case '*':   // Set secondary charset G2
                //case '+':   // Set secondary charset G3
                    _state = State::SPECIAL;
                    break;
                case '^':   // PM
                case '_':   // APC
                    _state = State::IGNORE;
                    break;
                default:
                    processEscape(lead);
                    break;
            }
            break;
        case State::CSI:
            // XXX Check for control characters here?
            // XXX deal with CSI flags?
            std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            if (isalpha(lead) || lead == '@' || lead == '`') {
                processCsi(_escSeq);
                _escSeq.clear();
                _state = State::NORMAL;
            }
            break;
        case State::INNER:
            if (lead == '\\') {
                if (_outerState == State::DCS) {
                    processDcs(_escSeq);
                }
                else if (_outerState == State::OSC) {
                    processOsc(_escSeq);
                }
                else {
                    // ??
                }
                _escSeq.clear();
                _state = State::NORMAL;
            }
            else if (lead == ESC) {
                _state = _outerState;
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            }
            else {
                _state = _outerState;
                _escSeq.push_back(ESC);
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            }
            break;
        case State::DCS:
        case State::OSC:
        case State::IGNORE:
            if (lead == ESC) {
                _outerState = _state;
                _state = State::INNER;
            }
            else if (lead == BEL && _state == State::OSC) {
                processOsc(_escSeq);
                _escSeq.clear();
                _state = State::NORMAL;
            }
            else {
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            }
            break;
        case State::SPECIAL:
            std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            if (isdigit(lead) || isalpha(lead)) {
                processSpecial(_escSeq);
            }
            _escSeq.clear();
            _state = State::NORMAL;
            break;
        default:
            break;
    }
}

void Terminal::processControl(char c) {
    switch (c) {
        case BEL:
            PRINT("BEL!!");
            break;
        case HT:
            // Advance to the next tab or the last column.
            // FIXME convert the empty cells into spaces
            for (; _cursorCol != _buffer.getCols(); ++_cursorCol) {
                if (_tabs[_cursorCol]) {
                    break;
                }
            }

            // Back up the cursor if we went past the end.
            if (_cursorCol == _buffer.getCols()) {
                --_cursorCol;
            }
            break;
        case BS:
            // TODO handle auto-wrap
            if (_cursorCol != 0) {
                --_cursorCol;
            }
            break;
        case CR:
            _cursorCol = 0;
            break;
        case LF:
            if (_modes.get(Mode::CRLF)) {
                _cursorCol = 0;
            }
            // Fall-through:
        case FF:
        case VT:
            if (_cursorRow == _buffer.getRows() - 1) {
                _buffer.addLine();
            }
            else {
                ++_cursorRow;
            }
            break;
        case SO:
            NYI("SO");
            break;
        case SI:
            NYI("SI");
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

void Terminal::processNormal(utf8::Seq seq, utf8::Length UNUSED(length)) {
    _buffer.set(_cursorRow, _cursorCol, Cell::utf8(seq, _attrs, _fg, _bg));

#if 0
    ++_cursorCol;

    if (_cursorCol == _buffer.getCols()) {
        if (_cursorRow == _buffer.getRows()) {
            _buffer.addLine();
        }
        else {
            ++_cursorRow;
        }
        _cursorCol = 0;
    }
#else
    _cursorCol = std::min(_buffer.getCols() - 1, _cursorCol + 1);
#endif

    _observer.terminalDamageAll();
}

void Terminal::processEscape(char c) {
    switch (c) {
        case 'D':   // IND - linefeed
            NYI("st.c:2169");
            break;
        case 'E':   // NEL - next line
            NYI("st.c:2177");
            break;
        case 'H':   // HTS - Horizontal tab stop.
            _tabs[_cursorCol] = true;
            break;
        case 'M':   // RI - Reverse index.
            NYI("st.c:2185");
            break;
        case 'Z':   // DECID - Identify Terminal
            NYI("st.c:2194");
            //ttywrite(VT102ID, sizeof(VT102ID) - 1);
            break;
        case 'c':   // RIS - Reset to initial state
            resetAll();
            break;
        case '=':   // DECPAM - Application keypad
            _modes.setTo(Mode::APPKEYPAD, true);
            break;
        case '>':   // DECPNM - Normal keypad
            _modes.setTo(Mode::APPKEYPAD, false);
            break;
        case '7':   // DECSC - Save Cursor
            NYI("st.c:2210");
            //tcursor(CURSOR_SAVE);
            break;
        case '8':   // DECRC - Restore Cursor
            NYI("st.c:2214");
            //tcursor(CURSOR_LOAD);
            break;
        default:
            ERROR("Unknown escape sequence: " << c);
            break;
    }
}

void Terminal::processCsi(const std::vector<char> & seq) {
    //PRINT("NYI:CSI: ESC" << Str(seq));

    ASSERT(seq.size() >= 2, "");

    size_t i = 0;
    bool priv = false;
    std::vector<int32_t> args;

    //
    // Parse the arguments.
    //

    ASSERT(seq[i] == '[', "");
    ++i;

    if (seq[i] == '?') {
        ++i;
        priv = true;
    }

    bool inArg = false;

    while (i != seq.size()) {
        char c = seq[i];

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

    ASSERT(i == seq.size() - 1, "i=" << i << ", seq.size=" << seq.size() << ", Seq: " << Str(seq));        // XXX replaces following conditional

    if (i == seq.size()) {
        ERROR("Bad CSI: " << Str(seq));
    }
    else {
        char mode = seq[i];
        switch (mode) {
            case '@': // ICH - Insert Character
                _buffer.insertCells(_cursorRow, _cursorCol, nthArg(args, 0, 1));
                _observer.terminalDamageAll();
                break;
            case 'A': // CUU - Cursor Up
                _cursorRow = clamp<uint16_t>(_cursorRow - nthArg(args, 0, 1), 0, _buffer.getRows() - 1);
                _observer.terminalDamageAll();
                break;
            case 'B': // CUD - Cursor Down
                _cursorRow = clamp<uint16_t>(_cursorRow + nthArg(args, 0, 1), 0, _buffer.getRows() - 1);
                _observer.terminalDamageAll();
                break;
            case 'C': // CUF - Cursor Forward
                _cursorCol = clamp<uint16_t>(_cursorCol + nthArg(args, 0, 1), 0, _buffer.getCols() - 1);
                _observer.terminalDamageAll();
                break;
            case 'D': // CUB - Cursor Backward
                _cursorCol = clamp<uint16_t>(_cursorCol - nthArg(args, 0, 1), 0, _buffer.getCols() - 1);
                _observer.terminalDamageAll();
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
                _cursorRow = clamp<uint16_t>(row, 0, _buffer.getRows() - 1);
                _cursorCol = clamp<uint16_t>(col, 0, _buffer.getCols() - 1);
                _observer.terminalDamageCells(_cursorRow, _cursorCol, _cursorCol + 1);
            }
                break;
            case 'I': // CHT - ??? Horizontal tab?
                NYI("Advance tab");
                break;
            case 'J': // ED - Erase Data
                // Clear screen.
                switch (nthArg(args, 0)) {
                    default:    // Correct default ??
                    case 0: // below
                        for (uint16_t r = _cursorRow + 1; r != _buffer.getRows(); ++r) {
                            _buffer.clearLine(r);
                        }
                        break;
                    case 1: // above
                        for (uint16_t r = 0; r != _cursorRow; ++r) {
                            _buffer.clearLine(r);
                        }
                        break;
                    case 2: // all
                        _buffer.clearAll();
                        _cursorRow = _cursorCol = 0;
                        break;
                }
                _observer.terminalDamageAll();
                break;
            case 'K':   // EL - Erase line
                switch (nthArg(args, 0)) {
                    default:    // Correct default ??
                    case 0: // right      FIXME or >2
                        // XXX is this right?
                        for (uint16_t c = _cursorCol; c != _buffer.getCols(); ++c) {
                            _buffer.set(_cursorRow, c, Cell::blank());
                        }
                        break;
                    case 1: // left
                        for (uint16_t c = 0; c != _cursorCol + 1; ++c) {
                            _buffer.set(_cursorRow, c, Cell::blank());
                        }
                        break;
                    case 2: // all
                        _buffer.clearLine(_cursorRow);
                        break;
                }
                _observer.terminalDamageAll();
                break;
            case 'L': // IL - Insert Lines
                _buffer.insertLines(_cursorRow, nthArg(args, 0, 1));
                _observer.terminalDamageAll();
                break;
            case 'M': // DL - Delete Lines
                _buffer.eraseLines(_cursorRow, nthArg(args, 0, 1));
                _observer.terminalDamageAll();
                break;
            case 'P': // DCH - ??? Delete Character???
                _buffer.eraseCells(_cursorRow, _cursorCol, nthArg(args, 0, 1));
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
                    std::ostringstream ost;
                    ost << ESC << '[' << _cursorRow << ';' << _cursorCol << 'R';
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
                        //_observer.interSetScrollTopBottom(top - 1, bottom - 1);
                    }
                    else {
                        //_observer.interSetScrollTop(top - 1);
                    }
                    //_observer.interMoveCursor(0, 0);
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
                PRINT("NYI:CSI: ESC" << Str(seq));
                break;
        }
    }
}

void Terminal::processDcs(const std::vector<char> & seq) {
    PRINT("NYI:DCS: " << Str(seq));
}

void Terminal::processOsc(const std::vector<char> & seq) {
    ASSERT(!seq.empty(), "");

    size_t i = 0;
    std::vector<std::string> args;

    //
    // Parse the arguments.
    //

    ASSERT(seq[i] == ']', "");
    ++i;

    bool next = true;
    while (i != seq.size()) {
        char c = seq[i];

        if (next) { args.push_back(std::string()); next = false; }

        if (c == ';') { next = true; }
        else          { args.back().push_back(c); }

        ++i;
    }

    //
    // Handle the sequence.
    //

    switch (unstringify<int>(nthArg(args, 0))) {
        case 0: // Icon name and window title
        case 1: // Icon label
        case 2: // Window title
            if (args.size() > 1) {      // XXX need if? we have fallback
                _observer.terminalSetTitle(nthArg(args, 1));
            }
            break;
        default:
            PRINT("Unandled: " << Str(seq));
    }
}

void Terminal::processSpecial(const std::vector<char> & seq) {
    PRINT("NYI:SPECIAL: " << Str(seq));
}

void Terminal::processAttributes(const std::vector<int32_t> & args) {
    for (size_t i = 0; i != args.size(); ++i) {
        int32_t v = args[i];

        switch (v) {
            case 0: // Reset/Normal
                _bg = Cell::defaultBg();
                _fg = Cell::defaultFg();
                _attrs = Cell::defaultAttrs();
                break;
            case 1: // Bold
                _attrs.set(Attribute::BOLD);
                break;
            case 2: // Faint (decreased intensity)
                NYI("Faint");
                break;
            case 3: // Italic: on
                _attrs.set(Attribute::ITALIC);
                break;
            case 4: // Underline: Single
                _attrs.set(Attribute::UNDERLINE);
                break;
            case 5: // Blink: slow
            case 6: // Blink: rapid
                _attrs.set(Attribute::BLINK);
                break;
            case 7: // Image: Negative
                _attrs.set(Attribute::REVERSE);
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
                _attrs.unset(Attribute::BOLD);
                break;
            case 23: // Not italic, not Fraktur
                _attrs.unset(Attribute::ITALIC);
                break;
            case 24: // Underline: None (not singly or doubly underlined)
                _attrs.unset(Attribute::UNDERLINE);
                break;
            case 25: // Blink: off
            case 26: // Reserved
                _attrs.unset(Attribute::BLINK);
                break;
            case 27: // Image: Positive
                _attrs.unset(Attribute::REVERSE);
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
                        _fg = v2;
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
                //_observer.interResetFg();
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
                        _bg = v2;
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
                _bg = Cell::defaultBg();
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
                    _fg = v - 30;
                }
                else if (v >= 40 && v < 48) {
                    // bright fg
                    _fg = v - 40;
                }
                else if (v >= 90 && v < 98) {
                    // normal bg
                    _bg = v - 90 + 8;
                }
                else if (v >= 100 && v < 108) {
                    // bright bg
                    _bg = v - 100 + 8;
                }
                // FOLLOWING STUFF UNTESTED:
                else if (v >= 256 && v < 512) {
                    _fg = v - 256;
                }
                else if (v >= 512 && v < 768) {
                    _bg = v - 512;
                }
                else {
                    ERROR("Unhandled attribute: " << v);
                }
                break;
        }
    }
}

void Terminal::processModes(bool priv, bool set, const std::vector<int32_t> & args) {
    //PRINT("processModes: priv=" << priv << ", set=" << set << ", args=" << strArgs(args));

    for (auto a : args) {
        if (priv) {
            switch (a) {
                case 1: // DECCKM - Cursor key
                    _modes.setTo(Mode::APPCURSOR, set);
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
                       MODBIT(term.mode, set, Mode::REVERSE);
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
                    //_observer.interSetMode(Mode::WRAP, set);
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
                    _modes.setTo(Mode::HIDE, !set);
                    break;
                case 42: // DECNRCM - National characters (IGNORED)
                    NYI("Ignored: "  << a);
                    break;
                case 1000: // 1000,1002: enable xterm mouse report
                    _modes.setTo(Mode::MOUSEBTN, set);
                    _modes.setTo(Mode::MOUSEMOTION, false);
                    break;
                case 1002:
                    _modes.setTo(Mode::MOUSEMOTION, set);
                    _modes.setTo(Mode::MOUSEBTN, false);
                    break;
                case 1006:
                    //_observer.interSetMode(Mode::MOUSESGR, set);
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
                    _modes.setTo(Mode::ALTSCREEN, set);
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
                    _modes.setTo(Mode::KBDLOCK, set);
                    break;
                case 4:  // IRM - Insertion-replacement
                    _modes.setTo(Mode::INSERT, set);
                    break;
                case 12: // SRM - Send/Receive
                    _modes.setTo(Mode::ECHO, set);
                    break;
                case 20: // LNM - Linefeed/new line
                    _modes.setTo(Mode::CRLF, set);
                    break;
                default:
                    ERROR("erresc: unknown set/reset mode: " <<  a);
                    break;
            }
        }
    }
}
