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

const Terminal::CharSub Terminal::CS_US[] = {
    { {}, {} }
};

const Terminal::CharSub Terminal::CS_UK[] = {
    { { '#' }, { 0xC2, 0xA3 } }, // POUND: £
    { {}, {} }
};

const Terminal::CharSub Terminal::CS_SPECIAL[] = {
    { { '`' }, { 0xE2, 0x99, 0xA6 } }, // diamond: ♦
    { { 'a' }, { 0xE2, 0x96, 0x92 } }, // 50% cell: ▒
    { { 'b' }, { 0xE2, 0x90, 0x89 } }, // HT: ␉
    { { 'c' }, { 0xE2, 0x90, 0x8C } }, // FF: ␌
    { { 'd' }, { 0xE2, 0x90, 0x8D } }, // CR: ␍
    { { 'e' }, { 0xE2, 0x90, 0x8A } }, // LF: ␊
    { { 'f' }, { 0xC2, 0xB0       } }, // Degree: °
    { { 'g' }, { 0xC2, 0xB1       } }, // Plus/Minus: ±
    { { 'h' }, { 0xE2, 0x90, 0xA4 } }, // NL: ␤
    { { 'i' }, { 0xE2, 0x90, 0x8B } }, // VT: ␋
    { { 'j' }, { 0xE2, 0x94, 0x98 } }, // CN_RB: ┘
    { { 'k' }, { 0xE2, 0x94, 0x90 } }, // CN_RT: ┐
    { { 'l' }, { 0xE2, 0x94, 0x8C } }, // CN_LT: ┌
    { { 'm' }, { 0xE2, 0x94, 0x94 } }, // CN_LB: └
    { { 'n' }, { 0xE2, 0x94, 0xBC } }, // CROSS: ┼
    { { 'o' }, { 0xE2, 0x8E, 0xBA } }, // Horiz. Scan Line 1: ⎺
    { { 'p' }, { 0xE2, 0x8E, 0xBB } }, // Horiz. Scan Line 3: ⎻
    { { 'q' }, { 0xE2, 0x94, 0x80 } }, // Horiz. Scan Line 5: ─
    { { 'r' }, { 0xE2, 0x8E, 0xBC } }, // Horiz. Scan Line 7: ⎼
    { { 's' }, { 0xE2, 0x8E, 0xBD } }, // Horiz. Scan Line 9: ⎽
    { { 't' }, { 0xE2, 0x94, 0x9C } }, // TR: ├
    { { 'u' }, { 0xE2, 0x94, 0xA4 } }, // TL: ┤
    { { 'v' }, { 0xE2, 0x94, 0xB4 } }, // TU: ┴
    { { 'w' }, { 0xE2, 0x94, 0xAC } }, // TD: ┬
    { { 'x' }, { 0xE2, 0x94, 0x82 } }, // V: │
    { { 'y' }, { 0xE2, 0x89, 0xA4 } }, // LE: ≤
    { { 'z' }, { 0xE2, 0x89, 0xA5 } }, // GE: ≥
    { { '{' }, { 0xCF, 0x80       } }, // PI: π
    { { '|' }, { 0xE2, 0x89, 0xA0 } }, // NEQ: ≠
    { { '}' }, { 0xC2, 0xA3       } }, // POUND: £
    { { '~' }, { 0xE2, 0x8B, 0x85 } }, // DOT: ⋅
    { {}, {} }
};

//
//
//

Terminal::Terminal(I_Observer   & observer,
                   const KeyMap & keyMap,
                   I_Tty        & tty,
                   uint16_t       rows,
                   uint16_t       cols,
                   bool           trace,
                   bool           sync) :
    _observer(observer),
    _dispatch(false),
    //
    _keyMap(keyMap),
    _priBuffer(rows, cols),
    _altBuffer(rows, cols),
    _buffer(&_priBuffer),
    //
    _G0(CS_US),
    _G1(CS_US),
    _CS(_G0),
    _cursorRow(0), _cursorCol(0),
    _bg(Cell::defaultBg()),
    _fg(Cell::defaultFg()),
    _attrs(Cell::defaultAttrs()),
    _originMode(false),
    //
    _modes(),
    _tabs(_buffer->getCols()),
    //
    _savedG0(_G0),
    _savedG1(_G1),
    _savedCS(_CS),
    _savedCursorRow(0),
    _savedCursorCol(0),
    _savedFg(0),
    _savedBg(0),
    _savedAttrs(),
    _savedOriginMode(false),
    //
    _tty(tty),
    _dumpWrites(false),
    _writeBuffer(),
    _state(State::NORMAL),
    _outerState(State::NORMAL),
    _escSeq(),
    _trace(trace),
    _sync(sync)
{
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }

    _modes.set(Mode::AUTO_WRAP);
    _modes.set(Mode::SHOW_CURSOR);
    _modes.set(Mode::AUTO_REPEAT);
    _modes.set(Mode::ALT_SENDS_ESC);
}

Terminal::~Terminal() {
    ASSERT(!_dispatch, "");
}

void Terminal::resize(uint16_t rows, uint16_t cols) {
    ASSERT(!_dispatch, "");
    ASSERT(rows > 0 && cols > 0, "");
    _cursorRow = std::min<uint16_t>(_cursorRow, rows - 1);
    _cursorCol = std::min<uint16_t>(_cursorCol, cols - 1);
    _priBuffer.resize(rows, cols);
    _altBuffer.resize(rows, cols);
    _tabs.resize(cols);
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }
}

void Terminal::damage(uint16_t rowBegin, uint16_t rowEnd,
                      uint16_t colBegin, uint16_t colEnd) {
    ENFORCE(_observer.terminalBeginFixDamage(false), "");
    draw(rowBegin, rowEnd, colBegin, colEnd, false);
    _observer.terminalEndFixDamage(false);
}

utf8::Seq Terminal::translate(utf8::Seq seq) const {
    for (const CharSub * cs = _CS; cs->match.bytes[0]; ++cs) {
        if (seq == cs->match) {
            if (_trace) {
                std::cerr
                    << Esc::BG_BLUE << Esc::FG_WHITE
                    << '/' << cs->match << '/' << cs->replace << '/'
                    << Esc::RESET;
            }
            FATAL("Match occurred");
            return cs->replace;
        }
    }
    return seq;
}

void Terminal::draw(uint16_t rowBegin, uint16_t rowEnd,
                    uint16_t colBegin, uint16_t colEnd,
                    bool internal) {
    // Declare run at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<char> run;
    // Reserve the largest amount of memory we could require.
    run.reserve(_buffer->getCols() * utf8::LMAX + 1);

    for (uint16_t r = rowBegin; r != rowEnd; ++r) {
        uint16_t     c_ = 0;    // Accumulation start column.
        uint8_t      fg = 0, bg = 0;
        AttributeSet attrs;
        uint16_t     c;
        uint16_t     colBegin2, colEnd2;

        if (internal) {
            _buffer->getDamage(r, colBegin2, colEnd2);
            //PRINT("Consulted buffer damage, got: " << colBegin2 << ", " << colEnd2);
        }
        else {
            colBegin2 = colBegin;
            colEnd2   = colEnd;
            //PRINT("External damage damage, got: " << colBegin2 << ", " << colEnd2);
        }

        for (c = colBegin2; c != colEnd2; ++c) {
            const Cell & cell = _buffer->getCell(r, c);

            if (run.empty() || fg != cell.fg() || bg != cell.bg() || attrs != cell.attrs()) {
                if (!run.empty()) {
                    // flush run
                    run.push_back(NUL);
                    _observer.terminalDrawRun(r, c_, fg, bg, attrs, &run.front(), c - c_);
                    run.clear();
                }

                c_    = c;
                fg    = cell.fg();
                bg    = cell.bg();
                attrs = cell.attrs();

                utf8::Length len = utf8::leadLength(cell.lead());
                run.resize(len);
                std::copy(cell.bytes(), cell.bytes() + len, &run.front());
            }
            else {
                size_t oldSize = run.size();
                utf8::Length len = utf8::leadLength(cell.lead());
                run.resize(run.size() + len);
                std::copy(cell.bytes(), cell.bytes() + len, &run[oldSize]);
            }
        }

        if (!run.empty()) {
            // flush run
            run.push_back(NUL);
            _observer.terminalDrawRun(r, c_, fg, bg, attrs, &run.front(), c - c_);
            run.clear();
        }
    }

    if (_modes.get(Mode::SHOW_CURSOR)) {
        // If the cursor is off the screen then draw it at the edge.
        uint16_t col =
            _cursorCol != _buffer->getCols() ? _cursorCol : _buffer->getCols() - 1;

        const Cell & cell = _buffer->getCell(_cursorRow, col);
        utf8::Length len  = utf8::leadLength(cell.lead());
        run.resize(len);
        std::copy(cell.bytes(), cell.bytes() + len, &run.front());
        run.push_back(NUL);
        _observer.terminalDrawCursor(_cursorRow, col,
                                     cell.fg(), cell.bg(), cell.attrs(), &run.front());
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

    try {
        Timer timer(50);
        char buf[BUFSIZ];        // 8192 last time I looked.
        do {
            size_t rval = _tty.read(buf, sizeof buf);
            if (rval == 0) { /*PRINT("Would block");*/ break; }
            //PRINT("Read: " << rval);
            processRead(buf, rval);
        } while (!timer.expired());
    }
    catch (I_Tty::Exited & ex) {
        _observer.terminalChildExited(ex.exitCode);
    }

    if (!_sync) {
        if (_observer.terminalBeginFixDamage(true)) {
            draw(0, _buffer->getRows(), 0, _buffer->getCols(), true);
            _observer.terminalEndFixDamage(true);
            _buffer->resetDamage();
        }
    }

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
    _buffer->clearAll();

    _cursorRow  = 0;
    _cursorCol  = 0;

    _bg         = Cell::defaultBg();
    _fg         = Cell::defaultFg();
    _attrs      = Cell::defaultAttrs();
    _originMode = false;

    _modes.clear();
    _modes.set(Mode::AUTO_WRAP);
    _modes.set(Mode::SHOW_CURSOR);
    _modes.set(Mode::AUTO_REPEAT);
    _modes.set(Mode::ALT_SENDS_ESC);

    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }

    _savedCursorRow  = _cursorRow;
    _savedCursorCol  = _cursorCol;

    _savedFg         = _fg;
    _savedBg         = _bg;
    _savedAttrs      = _attrs;
    _savedOriginMode = _originMode;

    _observer.terminalResetTitle();
}

void Terminal::processRead(const char * data, size_t size) {
    for (size_t i = 0; i != size; ++i) {
        switch (_utf8Machine.next(data[i])) {
            case utf8::Machine::State::ACCEPT: {
                processChar(_utf8Machine.seq(), _utf8Machine.length());
                break;
            }
            case utf8::Machine::State::REJECT:
                ERROR("Rejecting UTF-8 data.");
                break;
            default:
                break;
        }
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
                processNormal(seq);
            }
            break;
        case State::ESCAPE:
            ASSERT(_escSeq.empty(), "");
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
                    _escSeq.clear();
                    _state = State::NORMAL;
                    break;
            }
            break;
        case State::CSI:
            ASSERT(!_escSeq.empty(), "");
            if (lead >= NUL && lead < SPACE) {
                ASSERT(len == utf8::Length::L1, "");
                processControl(lead);
            }
            else if (lead == '?') {
                // XXX For now put the '?' into _escSeq because
                // processCsi is expecting it.
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
                //terminal->escape_flags |= ESC_FLAG_WHAT;
            }
            else if (lead == '>') {
                //terminal->escape_flags |= ESC_FLAG_GT;
            }
            else if (lead == '!') {
                //terminal->escape_flags |= ESC_FLAG_BANG;
            }
            else if (lead == '$') {
                //terminal->escape_flags |= ESC_FLAG_CASH;
            }
            else if (lead == '\'') {
                //terminal->escape_flags |= ESC_FLAG_SQUOTE;
            }
            else if (lead == '"') {
                //terminal->escape_flags |= ESC_FLAG_DQUOTE;
            }
            else if (lead == ' ') {
                //terminal->escape_flags |= ESC_FLAG_SPACE;
            }
            else {
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            }

            if (isalpha(lead) || lead == '@' || lead == '`') {
                processCsi(_escSeq);
                _escSeq.clear();
                _state = State::NORMAL;
            }
            break;
        case State::INNER:
            // XXX check the logic for INNER
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
                // XXX reset _outerState?
            }
            else if (lead == ESC) {
                _state = _outerState;
                // XXX reset _outerState?
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            }
            else {
                _state = _outerState;
                // XXX reset _outerState?
                _escSeq.push_back(ESC);
                std::copy(seq.bytes, seq.bytes + len, std::back_inserter(_escSeq));
            }
            break;
        case State::DCS:
        case State::OSC:
        case State::IGNORE:
            ASSERT(!_escSeq.empty(), "");
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
            ASSERT(!_escSeq.empty(), "");
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

    if (_sync) {
        if (_observer.terminalBeginFixDamage(true)) {
            draw(0, _buffer->getRows(), 0, _buffer->getCols(), true);
            _observer.terminalEndFixDamage(true);
            _buffer->resetDamage();
        }
    }
}

void Terminal::processControl(char c) {
    if (_trace) {
        std::cerr << Esc::FG_YELLOW << Char(c) << Esc::RESET;
    }

    switch (c) {
        case BEL:
            PRINT("BEL!!");
            break;
        case HT:
            _buffer->damageCursor(_cursorRow, _cursorCol);

            // Advance to the next tab or the last column.
            for (; _cursorCol != _buffer->getCols(); ++_cursorCol) {
                if (_tabs[_cursorCol]) {
                    break;
                }
            }

            // Back up the cursor if we went past the end.
            if (_cursorCol == _buffer->getCols()) {
                --_cursorCol;
            }
            break;
        case BS:
            _buffer->damageCursor(_cursorRow, _cursorCol);

            // XXX check this
            if (_cursorCol == 0) {
                if (_modes.get(Mode::AUTO_WRAP)) {
                    if (_cursorRow > _buffer->getScrollBegin()) {
                        _cursorCol = _buffer->getCols() - 1;
                        --_cursorRow;
                    }
                }
            }
            else {
                --_cursorCol;
            }
            break;
        case CR:
            _buffer->damageCursor(_cursorRow, _cursorCol);
            _cursorCol = 0;
            break;
        case LF:
            if (_modes.get(Mode::CRLF)) {
                _buffer->damageCursor(_cursorRow, _cursorCol);
                _cursorCol = 0;
            }
            // Fall-through:
        case FF:
        case VT:
            if (_cursorRow == _buffer->getScrollEnd() - 1) {
                if (_trace) {
                    std::cerr << "(ADDLINE1)" << std::endl;
                }
                _buffer->addLine();
            }
            else {
                _buffer->damageCursor(_cursorRow, _cursorCol);
                ++_cursorRow;
            }

            if (_trace) {
                std::cerr << std::endl;
            }
            break;
        case SO:
            PRINT("\n *** G1");
            _CS = _G1;
            break;
        case SI:
            PRINT("\n *** G0");
            _CS = _G0;
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

void Terminal::processNormal(utf8::Seq seq) {
    seq = translate(seq);

    if (_trace) {
        std::cerr << Esc::FG_GREEN << Esc::UNDERLINE << seq << Esc::RESET;
    }

    _buffer->damageCursor(_cursorRow, _cursorCol);

    if (_cursorCol == _buffer->getCols()) {
        if (_modes.get(Mode::AUTO_WRAP)) {
            _cursorCol = 0;
            if (_cursorRow == _buffer->getScrollEnd() - 1) {
                _buffer->addLine();
            }
            else {
                ++_cursorRow;
            }
        }
        else {
            --_cursorCol;
        }
    }

    ASSERT(_cursorCol < _buffer->getCols(), "");
    ASSERT(_cursorRow < _buffer->getRows(), "");

    _buffer->setCell(_cursorRow, _cursorCol, Cell::utf8(seq, _attrs, _fg, _bg));
    ++_cursorCol;
}

void Terminal::processEscape(char c) {
    if (_trace) {
        std::cerr << Esc::FG_MAGENTA << "ESC" << Char(c) << Esc::RESET << " ";
    }

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
            if (_cursorRow == 0) {
                // FIXME
                _buffer->insertLines(0, 1);
            }
            else {
                _buffer->damageCursor(_cursorRow, _cursorCol);
                --_cursorRow;
            }
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
            _savedG0         = _G0;
            _savedG1         = _G1;
            _savedCS         = _CS;
            _savedCursorRow  = _cursorRow;
            _savedCursorCol  = _cursorCol;
            _savedFg         = _fg;
            _savedBg         = _bg;
            _savedAttrs      = _attrs;
            _savedOriginMode = _originMode;
            break;
        case '8':   // DECRC - Restore Cursor
            _buffer->damageCursor(_cursorRow, _cursorCol);

            _G0         = _savedG0;
            _G1         = _savedG1;
            _CS         = _savedCS;
            _cursorRow  = _savedCursorRow;
            _cursorCol  = _savedCursorCol;
            _fg         = _savedFg;
            _bg         = _savedBg;
            _attrs      = _savedAttrs;
            _originMode = _savedOriginMode;
            break;
        default:
            ERROR("Unknown escape sequence: ESC" << Char(c));
            break;
    }
}

void Terminal::processCsi(const std::vector<char> & seq) {
    ASSERT(seq.size() >= 2, "");

    if (_trace) {
        std::cerr << Esc::FG_CYAN << "ESC" << Str(seq) << Esc::RESET << " ";
    }

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
                _buffer->insertCells(_cursorRow, _cursorCol, nthArg(args, 0, 1));
                break;
            case 'A': // CUU - Cursor Up
                _buffer->damageCursor(_cursorRow, _cursorCol);
                _cursorRow = clamp<int32_t>(_cursorRow - nthArg(args, 0, 1), 0, _buffer->getRows() - 1);
                break;
            case 'B': // CUD - Cursor Down
                _buffer->damageCursor(_cursorRow, _cursorCol);
                _cursorRow = clamp<int32_t>(_cursorRow + nthArg(args, 0, 1), 0, _buffer->getRows() - 1);
                break;
            case 'C': // CUF - Cursor Forward
                _buffer->damageCursor(_cursorRow, _cursorCol);
                _cursorCol = clamp<int32_t>(_cursorCol + nthArg(args, 0, 1), 0, _buffer->getCols() - 1);
                break;
            case 'D': // CUB - Cursor Backward
                _buffer->damageCursor(_cursorRow, _cursorCol);
                _cursorCol = clamp<int32_t>(_cursorCol - nthArg(args, 0, 1), 0, _buffer->getCols() - 1);
                break;
            case 'E': // CNL - Cursor Next Line
                NYI("CNL");
                break;
            case 'F': // CPL - Cursor Previous Line
                NYI("CPL");
                break;
            case 'G': // CHA - Cursor Horizontal Absolute
                // TODO damage
                _cursorCol = clamp<uint16_t>(nthArg(args, 0, 1), 1, _buffer->getCols()) - 1;
                break;
            case 'f':       // HVP - Horizontal and Vertical Position
            case 'H': {     // CUP - Cursor Position
                if (_trace) {
                    std::cerr << std::endl;
                }

                _buffer->damageCursor(_cursorRow, _cursorCol);

                uint16_t row = nthArg(args, 0, 1) - 1;
                uint16_t col = nthArg(args, 1, 1) - 1;
                _cursorRow = clamp<int32_t>(row, 0, _buffer->getRows() - 1);
                _cursorCol = clamp<int32_t>(col, 0, _buffer->getCols() - 1);
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
                        for (uint16_t r = _cursorRow + 1; r != _buffer->getRows(); ++r) {
                            _buffer->clearLine(r);
                        }
                        break;
                    case 1: // above
                        for (uint16_t r = 0; r != _cursorRow; ++r) {
                            _buffer->clearLine(r);
                        }
                        break;
                    case 2: // all
                        _buffer->clearAll();
                        _cursorRow = _cursorCol = 0;
                        break;
                }
                break;
            case 'K':   // EL - Erase line
                switch (nthArg(args, 0)) {
                    default:    // Correct default ??
                    case 0: // right      FIXME or >2
                        // XXX is this right?
                        for (uint16_t c = _cursorCol; c != _buffer->getCols(); ++c) {
                            _buffer->setCell(_cursorRow, c, Cell::blank());
                        }
                        break;
                    case 1: // left
                        for (uint16_t c = 0; c != _cursorCol + 1; ++c) {
                            _buffer->setCell(_cursorRow, c, Cell::blank());
                        }
                        break;
                    case 2: // all
                        _buffer->clearLine(_cursorRow);
                        break;
                }
                break;
            case 'L': // IL - Insert Lines
                _buffer->insertLines(_cursorRow, nthArg(args, 0, 1));
                break;
            case 'M': // DL - Delete Lines
                _buffer->eraseLines(_cursorRow, nthArg(args, 0, 1));
                break;
            case 'P': // DCH - ??? Delete Character???
                _buffer->eraseCells(_cursorRow, _cursorCol, nthArg(args, 0, 1));
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
                // TODO damage
                _cursorCol = clamp<uint16_t>(nthArg(args, 0, 1), 1, _buffer->getCols()) - 1;
                break;

            case 'b': // REP
                NYI("REP");
                break;
            case 'c': // Primary DA
                write("\x1B[?6c", 5);
                break;
            case 'd': // VPA
                _cursorRow = clamp<uint16_t>(nthArg(args, 0, 1), 1, _buffer->getRows()) - 1;
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
                if (args.empty()) {
                    args.push_back(0);
                }
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
            case 'q': // DECSCA - Select Character Protection Attribute
                NYI("DECSCA");
                break;
            case 'r': // DECSTBM - Set Top and Bottom Margins (scrolling)
                if (priv) {
                    goto default_;
                }
                else {
                    if (args.empty()) {
                        _buffer->resetScrollBeginEnd();
                        _buffer->damageCursor(_cursorRow, _cursorCol);
                        _cursorRow = _cursorCol = 0;
                    }
                    else {
                        // http://www.vt100.net/docs/vt510-rm/DECSTBM
                        int32_t top    = nthArg(args, 0, 1) - 1;
                        int32_t bottom = nthArg(args, 1, 1) - 1;

                        top    = clamp<int32_t>(top,    0, _buffer->getRows() - 1);
                        bottom = clamp<int32_t>(bottom, 0, _buffer->getRows() - 1);

                        if (bottom > top) {
                            _buffer->setScrollBeginEnd(top, bottom + 1);
                        }
                        else {
                            _buffer->resetScrollBeginEnd();
                        }

                        _buffer->damageCursor(_cursorRow, _cursorCol);

                        if (_originMode) {
                            _cursorRow = top;
                        }
                        else {
                            _cursorRow = 0;
                        }

                        _cursorCol = 0;
                    }
                }
                break;
            case 's': // save cursor
                _savedCursorRow = _cursorRow;
                _savedCursorCol = _cursorCol;
                break;
            case 't': // window ops?
                NYI("Window ops");
                break;
            case 'u': // restore cursor
                _buffer->damageCursor(_cursorRow, _cursorCol);
                _cursorRow = _savedCursorRow;
                _cursorCol = _savedCursorCol;
                break;
default_:
            default:
                PRINT("NYI:CSI: ESC" << Str(seq));
                break;
        }
    }
}

void Terminal::processDcs(const std::vector<char> & seq) {
    if (_trace) {
        std::cerr << Esc::FG_RED << "ESC" << Str(seq) << Esc::RESET << " ";
    }
}

void Terminal::processOsc(const std::vector<char> & seq) {
    if (_trace) {
        std::cerr << Esc::FG_MAGENTA << "ESC" << Str(seq) << Esc::RESET << " ";
    }

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
            break;
    }
}

void Terminal::processSpecial(const std::vector<char> & seq) {
    if (_trace) {
        std::cerr << Esc::FG_BLUE << "ESC" << Str(seq) << Esc::RESET << " ";
    }

    ASSERT(seq.size() == 2, "");
    char special = seq.front();
    char code    = seq.back();

    switch (special) {
        case '#':
            switch (code) {
                case '8':
                    // Fill terminal with 'E'
                    for (uint16_t r = 0; r != _buffer->getRows(); ++r) {
                        for (uint16_t c = 0; c != _buffer->getRows(); ++c) {
                            _buffer->setCell(r, c, Cell::utf8(utf8::Seq('E'), _attrs, _fg, _bg));
                        }
                    }
                    break;
                default:
                    NYI("?");
                    break;
            }
            break;
        case '(':
            switch (code) {
                case '0':
                    PRINT("\nG0 = SPECIAL");
                    _G0 = CS_SPECIAL;
                    break;
                case 'A':
                    PRINT("\nG0 = UK");
                    _G0 = CS_UK;
                    break;
                case 'B':
                    PRINT("\nG0 = US");
                    _G0 = CS_US;
                    break;
                default:
                    NYI("Unknown character set: " << code);
                    break;
            }
            break;
        case ')':
            switch (code) {
                case '0':
                    PRINT("\nG1 = SPECIAL");
                    _G1 = CS_SPECIAL;
                    break;
                case 'A':
                    PRINT("\nG1 = UK");
                    _G1 = CS_UK;
                    break;
                case 'B':
                    PRINT("\nG1 = US");
                    _G1 = CS_US;
                    break;
                default:
                    NYI("Unknown character set: " << code);
                    break;
            }
            break;
        default:
            NYI("Special: " << Str(seq));
            break;
    }
}

void Terminal::processAttributes(const std::vector<int32_t> & args) {
    ASSERT(!args.empty(), "");

    for (size_t i = 0; i != args.size(); ++i) {
        int32_t v = args[i];

        switch (v) {
            case 0: // Reset/Normal
                _bg    = Cell::defaultBg();
                _fg    = Cell::defaultFg();
                _attrs = Cell::defaultAttrs();
                break;
            case 1: // Bold
                _attrs.set(Attribute::BOLD);
                if (_fg < 8) { _fg += 8; }      // Normal -> Bright.
                break;
            case 2: // Faint (decreased intensity)
                _attrs.unset(Attribute::BOLD);
                if (_fg >= 8 && _fg < 16) { _fg -= 8; } // Bright -> Normal.
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
                _attrs.set(Attribute::INVERSE);
                break;
            case 8: // Conceal (not widely supported)
                _attrs.set(Attribute::CONCEAL);
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
                _attrs.unset(Attribute::BOLD);
                if (_fg >= 8 && _fg < 16) { _fg -= 8; } // Bright -> Normal.
                break;
            case 22: // Normal color or intensity (neither bold nor faint)
                _attrs.unset(Attribute::BOLD);
                if (_fg >= 8 && _fg < 16) { _fg -= 8; } // Bright -> Normal.
                break;
            case 23: // Not italic, not Fraktur
                _attrs.unset(Attribute::ITALIC);
                break;
            case 24: // Underline: None (not singly or doubly underlined)
                _attrs.unset(Attribute::UNDERLINE);
                break;
            case 25: // Blink: off
                _attrs.unset(Attribute::BLINK);
                break;
            case 26: // Reserved?
                _attrs.set(Attribute::INVERSE);
                break;
            case 27: // Image: Positive
                _attrs.unset(Attribute::INVERSE);
                break;
            case 28: // Reveal (conceal off - not widely supported)
                _attrs.unset(Attribute::CONCEAL);
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
                _fg = Cell::defaultFg();
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
                    // BOLD -> += 8
                }
                else if (v >= 40 && v < 48) {
                    // normal bg
                    _bg = v - 40;
                }
                else if (v >= 90 && v < 98) {
                    // bright fg
                    _fg = v - 90 + 8;
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
                case 1: // DECCKM - Cursor Keys Mode
                    _modes.setTo(Mode::APPCURSOR, set);
                    break;
                case 2: // DECANM - ANSI/VT52 Mode
                    NYI("DECANM: " << set);
                    _G0 = CS_US;
                    _G1 = CS_US;
                    _CS = _G0;
                    break;
                case 3: // DECCOLM - Column
                    if (set) {
                        // resize 132x24
                    }
                    else {
                        // resize 80x24
                    }
                    NYI("DECCOLM: " << set);
                    break;
                case 4:  // DECSCLM - Scroll (IGNORED)
                    NYI("DECSCLM: " << set);
                    break;
                case 5: // DECSCNM - Screen Mode: Light or Dark background.
                    NYI("DECSCNM: " << set);
                    break;
                case 6: // DECOM - Origin
                    _originMode = set;

                    _buffer->damageCursor(_cursorRow, _cursorCol);

                    if (_originMode) {
                        _cursorRow = _buffer->getScrollBegin();
                    }
                    else {
                        _cursorRow = 0;
                    }

                    _cursorCol = 0;
                    break;
                case 7: // DECAWM - Auto wrap
                    _modes.setTo(Mode::AUTO_WRAP, set);
                    break;
                case 8:  // DECARM - Auto repeat
                    _modes.setTo(Mode::AUTO_REPEAT, set);
                    break;
                case 9:
                    // Inter lace
                    break;
                case 12: // CVVIS/att610 - Cursor Very Visible.
                    //NYI("CVVIS/att610: " << set);
                    break;
                case 18: // DECPFF - Printer feed (IGNORED)
                case 19: // DECPEX - Printer extent (IGNORED)
                    NYI("DECPFF/DECPEX: " << set);
                    break;
                case 25: // DECTCEM - Text Cursor Enable Mode
                    _modes.setTo(Mode::SHOW_CURSOR, set);
                    break;
                case 40:
                    // Allow column
                    break;
                case 42: // DECNRCM - National characters (IGNORED)
                    NYI("Ignored: "  << a << ", " << set);
                    break;
                case 1000: // 1000,1002: enable xterm mouse report
                    _modes.setTo(Mode::MOUSEBTN, set);
                    _modes.setTo(Mode::MOUSEMOTION, false);
                    break;
                case 1002:
                    _modes.setTo(Mode::MOUSEMOTION, set);
                    _modes.setTo(Mode::MOUSEBTN, false);
                    break;
                case 1004:
                    // ??? tmux REPORT FOCUS
                    break;
                case 1005:
                    // ??? tmux MOUSE FORMAT = XTERM EXT
                    break;
                case 1006:
                    // MOUSE FORMAT = STR
                    _modes.setTo(Mode::MOUSESGR, set);
                    break;
                case 1015:
                    // MOUSE FORMAT = URXVT
                    break;
                case 1034: // ssm/rrm, meta mode on/off
                    NYI("1034: " << set);
                    break;
                case 1037: // deleteSendsDel
                    _modes.setTo(Mode::DELETE_SENDS_DEL, set);
                    break;
                case 1039: // altSendsEscape
                    _modes.setTo(Mode::ALT_SENDS_ESC, set);
                    break;
                case 1049: // rmcup/smcup, alternative screen
                case 47:    // XXX ???
                case 1047:
                    if (_buffer == &_altBuffer) {
                        _buffer->clearAll();
                    }

                    _buffer = set ? &_altBuffer : &_priBuffer;
                    if(a != 1049) {
                        _buffer->damageAll();
                        break;
                    }
                    // Fall-through:
                case 1048:
                    if (set) {
                        _savedCursorRow  = _cursorRow;
                        _savedCursorCol  = _cursorCol;
                        _savedFg         = _fg;
                        _savedBg         = _bg;
                        _savedAttrs      = _attrs;
                        _savedOriginMode = _originMode;
                    }
                    else {
                        _buffer->damageCursor(_cursorRow, _cursorCol);

                        _cursorRow  = _savedCursorRow;
                        _cursorCol  = _savedCursorCol;
                        _fg         = _savedFg;
                        _bg         = _savedBg;
                        _attrs      = _savedAttrs;
                        _originMode = _savedOriginMode;
                    }
                    _buffer->damageAll();
                    break;
                case 2004:
                    // Bracketed paste mode.
                    break;
                default:
                    ERROR("erresc: unknown private set/reset mode : " << a);
                    break;
            }
        }
        else {
            switch (a) {
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
