// vi:noai:sw=4

#include "terminol/common/terminal.hxx"

#include <algorithm>

namespace {

/*
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
*/

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

const uint16_t Terminal::TAB_SIZE = 8;

const Terminal::CharSub Terminal::CS_US[] = {
    { NUL, {} }
};

const Terminal::CharSub Terminal::CS_UK[] = {
    { '#', { 0xC2, 0xA3 } }, // POUND: £
    { NUL, {} }
};

const Terminal::CharSub Terminal::CS_SPECIAL[] = {
    { '`', { 0xE2, 0x99, 0xA6 } }, // diamond: ♦
    { 'a', { 0xE2, 0x96, 0x92 } }, // 50% cell: ▒
    { 'b', { 0xE2, 0x90, 0x89 } }, // HT: ␉
    { 'c', { 0xE2, 0x90, 0x8C } }, // FF: ␌
    { 'd', { 0xE2, 0x90, 0x8D } }, // CR: ␍
    { 'e', { 0xE2, 0x90, 0x8A } }, // LF: ␊
    { 'f', { 0xC2, 0xB0       } }, // Degree: °
    { 'g', { 0xC2, 0xB1       } }, // Plus/Minus: ±
    { 'h', { 0xE2, 0x90, 0xA4 } }, // NL: ␤
    { 'i', { 0xE2, 0x90, 0x8B } }, // VT: ␋
    { 'j', { 0xE2, 0x94, 0x98 } }, // CN_RB: ┘
    { 'k', { 0xE2, 0x94, 0x90 } }, // CN_RT: ┐
    { 'l', { 0xE2, 0x94, 0x8C } }, // CN_LT: ┌
    { 'm', { 0xE2, 0x94, 0x94 } }, // CN_LB: └
    { 'n', { 0xE2, 0x94, 0xBC } }, // CROSS: ┼
    { 'o', { 0xE2, 0x8E, 0xBA } }, // Horiz. Scan Line 1: ⎺
    { 'p', { 0xE2, 0x8E, 0xBB } }, // Horiz. Scan Line 3: ⎻
    { 'q', { 0xE2, 0x94, 0x80 } }, // Horiz. Scan Line 5: ─
    { 'r', { 0xE2, 0x8E, 0xBC } }, // Horiz. Scan Line 7: ⎼
    { 's', { 0xE2, 0x8E, 0xBD } }, // Horiz. Scan Line 9: ⎽
    { 't', { 0xE2, 0x94, 0x9C } }, // TR: ├
    { 'u', { 0xE2, 0x94, 0xA4 } }, // TL: ┤
    { 'v', { 0xE2, 0x94, 0xB4 } }, // TU: ┴
    { 'w', { 0xE2, 0x94, 0xAC } }, // TD: ┬
    { 'x', { 0xE2, 0x94, 0x82 } }, // V: │
    { 'y', { 0xE2, 0x89, 0xA4 } }, // LE: ≤
    { 'z', { 0xE2, 0x89, 0xA5 } }, // GE: ≥
    { '{', { 0xCF, 0x80       } }, // PI: π
    { '|', { 0xE2, 0x89, 0xA0 } }, // NEQ: ≠
    { '}', { 0xC2, 0xA3       } }, // POUND: £
    { '~', { 0xE2, 0x8B, 0x85 } }, // DOT: ⋅
    { NUL, {} }
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
    _priBuffer(rows, cols, std::numeric_limits<size_t>::max()),
    _altBuffer(rows, cols, 0),
    _buffer(&_priBuffer),
    //
    _otherCharSet(false),
    _G0(CS_US),
    _G1(CS_US),
    _cursorRow(0), _cursorCol(0),
    _fg(Cell::defaultFg()),
    _bg(Cell::defaultBg()),
    _attrs(Cell::defaultAttrs()),
    _originMode(false),
    //
    _modes(),
    _tabs(_buffer->getCols()),
    //
    _savedOtherCharSet(_otherCharSet),
    _savedG0(_G0),
    _savedG1(_G1),
    _savedCursorRow(0),
    _savedCursorCol(0),
    _savedFg(0),
    _savedBg(0),
    _savedAttrs(),
    _savedOriginMode(false),
    //
    _damageRowBegin(0),
    _damageRowEnd(0),
    _damageColBegin(0),
    _damageColEnd(0),
    //
    _tty(tty),
    _dumpWrites(false),
    _writeBuffer(),
    _utf8Machine(),
    _vtMachine(*this),
    _trace(trace),
    _sync(sync)
{
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % TAB_SIZE == 0;
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
        _tabs[i] = (i + 1) % TAB_SIZE == 0;
    }
}

void Terminal::redraw(uint16_t rowBegin, uint16_t rowEnd,
                      uint16_t colBegin, uint16_t colEnd) {
    fixDamage(rowBegin, rowEnd, colBegin, colEnd, Damage::EXPOSURE);
}

void Terminal::keyPress(xkb_keysym_t keySym, uint8_t state) {
    if (!handleKeyBinding(keySym, state) && _keyMap.isPotent(keySym)) {
        if (/* scroll on keystroke && */ _buffer->scrollBottom()) {
            fixDamage(0, _buffer->getRows(),
                      0, _buffer->getCols(),
                      Damage::SCROLL);
        }

        std::vector<uint8_t> str;
        if (_keyMap.convert(keySym, state,
                            _modes.get(Mode::APPKEYPAD),
                            _modes.get(Mode::APPCURSOR),
                            _modes.get(Mode::CRLF),
                            str)) {
            write(&str.front(), str.size());
        }
    }
}

void Terminal::scrollWheel(ScrollDir dir) {
    switch (dir) {
        case ScrollDir::UP:
            // TODO consolidate scroll operations with method.
            if (_buffer->scrollUp(std::max(1, _buffer->getRows() / 4))) {
                fixDamage(0, _buffer->getRows(),
                          0, _buffer->getCols(),
                          Damage::SCROLL);
            }
            break;
        case ScrollDir::DOWN:
            if (_buffer->scrollDown(std::max(1, _buffer->getRows() / 4))) {
                fixDamage(0, _buffer->getRows(),
                          0, _buffer->getCols(),
                          Damage::SCROLL);
            }
            break;
    }
}

void Terminal::read() {
    ASSERT(!_dispatch, "");

    _dispatch = true;

    try {
        const int FPS = 50;             // Frames per second
        Timer     timer(1000 / FPS);
        uint8_t   buf[BUFSIZ];          // 8192 last time I looked.
        do {
            //static int i = 0;
            //PRINT("Read: " << ++i);
            size_t rval = _tty.read(buf, _sync ? 16 : sizeof buf);
            if (rval == 0) { /*PRINT("Would block");*/ break; }
            //PRINT("Read: " << rval);
            processRead(buf, rval);
        } while (!timer.expired());

        //PRINT("Exit loop");
    }
    catch (I_Tty::Exited & ex) {
        _observer.terminalChildExited(ex.exitCode);
    }

    if (!_sync) {
        fixDamage(0, _buffer->getRows(), 0, _buffer->getCols(), Damage::TTY);
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

bool Terminal::handleKeyBinding(xkb_keysym_t keySym, uint8_t state) {
    if (state & _keyMap.maskShift()) {
        switch (keySym) {
            case XKB_KEY_Up:
                if (_buffer->scrollUp(1)) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                }
                return true;
            case XKB_KEY_Down:
                if (_buffer->scrollDown(1)) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                }
                return true;
            case XKB_KEY_Page_Up:
                if (_buffer->scrollUp(_buffer->getRows())) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                }
                return true;
            case XKB_KEY_Page_Down:
                if (_buffer->scrollDown(_buffer->getRows())) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                }
                return true;
            case XKB_KEY_Home:
                if (_buffer->scrollTop()) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                }
                return true;
            case XKB_KEY_End:
                if (_buffer->scrollBottom()) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                }
                return true;
            default:
                return false;
        }
    }
    else {
        return false;
    }
}

void Terminal::moveCursor(int32_t row, int32_t col) {
    damageCursor();
    _cursorRow = clamp<int32_t>(row, 0, _buffer->getRows() - 1);
    _cursorCol = clamp<int32_t>(col, 0, _buffer->getCols() - 1);
}

void Terminal::tabCursor(TabDir dir, uint16_t count) {
    damageCursor();

    switch (dir) {
        case TabDir::FORWARD:
            while (count != 0) {
                ++_cursorCol;

                if (_cursorCol == _buffer->getCols()) {
                    --_cursorCol;
                    break;
                }

                if (_tabs[_cursorCol]) {
                    --count;
                }
            }
            break;
        case TabDir::BACKWARD:
            while (count != 0) {
                if (_cursorCol == 0) {
                    break;
                }

                --_cursorCol;

                if (_tabs[_cursorCol]) {
                    --count;
                }
            }
            break;
    }
}

void Terminal::damageCursor() {
    uint16_t col = _cursorCol;
    if (col == _buffer->getCols()) { --col; }
    _buffer->damageCell(_cursorRow, col);
}

void Terminal::fixDamage(uint16_t rowBegin, uint16_t rowEnd,
                         uint16_t colBegin, uint16_t colEnd,
                         Damage damage) {
    if (_observer.terminalBeginFixDamage(damage != Damage::EXPOSURE)) {
        draw(rowBegin, rowEnd, colBegin, colEnd, damage);
        _observer.terminalEndFixDamage(damage != Damage::EXPOSURE,
                                       _damageRowBegin, _damageRowEnd,
                                       _damageColBegin, _damageColEnd);

        if (damage == Damage::TTY) {
            _buffer->resetDamage();
        }
    }
    else {
        // If we received a redraw() then the observer had better be able
        // to handle it.
        ENFORCE(damage != Damage::EXPOSURE, "");
    }
}

utf8::Seq Terminal::translate(utf8::Seq seq, utf8::Length length) const {
    if (length == utf8::Length::L1) {
        uint8_t ascii = seq.bytes[0];
        for (const CharSub * cs = _otherCharSet ? _G1 : _G0; cs->match != NUL; ++cs) {
            if (ascii == cs->match) {
                if (_trace) {
                    std::cerr
                        << Esc::BG_BLUE << Esc::FG_WHITE
                        << '/' << cs->match << '/' << cs->replace << '/'
                        << Esc::RESET;
                }
                return cs->replace;
            }
        }
    }

    return seq;
}

void Terminal::draw(uint16_t rowBegin, uint16_t rowEnd,
                    uint16_t colBegin, uint16_t colEnd,
                    Damage damage) {
    _damageRowBegin = 0;
    _damageRowEnd   = 0;
    _damageColBegin = 0;
    _damageColEnd   = 0;

    // Declare run at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<uint8_t> run;
    // Reserve the largest amount of memory we could require.
    run.reserve(_buffer->getCols() * utf8::Length::LMAX + 1);

    for (uint16_t r = rowBegin; r != rowEnd; ++r) {
        uint16_t     c_ = 0;    // Accumulation start column.
        uint8_t      fg = 0, bg = 0;
        AttributeSet attrs;
        uint16_t     c;
        uint16_t     colBegin2, colEnd2;

        if (damage == Damage::TTY) {
            _buffer->getDamage(r, colBegin2, colEnd2);
            //PRINT("Consulted buffer damage, got: " << colBegin2 << ", " << colEnd2);
        }
        else {
            colBegin2 = colBegin;
            colEnd2   = colEnd;
            //PRINT("External damage damage, got: " << colBegin2 << ", " << colEnd2);
        }

        // Update _damageColBegin and _damageColEnd.
        if (_damageColBegin == _damageColEnd) {
            _damageColBegin = colBegin2;
            _damageColEnd   = colEnd2;
        }
        else if (colBegin2 != colEnd2) {
            _damageColBegin = std::min(_damageColBegin, colBegin2);
            _damageColEnd   = std::max(_damageColEnd,   colEnd2);
        }

        // Update _damageRowBegin and _damageRowEnd.
        if (colBegin2 != colEnd2) {
            if (_damageRowBegin == _damageRowEnd) {
                _damageRowBegin = r;
            }

            _damageRowEnd = r + 1;
        }

        // Step through each column, accumulating runs of compatible consecutive cells.
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

                utf8::Length length = utf8::leadLength(cell.lead());
                run.resize(length);
                std::copy(cell.bytes(), cell.bytes() + length, &run.front());
            }
            else {
                size_t oldSize = run.size();
                utf8::Length length = utf8::leadLength(cell.lead());
                run.resize(run.size() + length);
                std::copy(cell.bytes(), cell.bytes() + length, &run[oldSize]);
            }
        }

        // There may be an unterminated run to flush.
        if (!run.empty()) {
            run.push_back(NUL);
            _observer.terminalDrawRun(r, c_, fg, bg, attrs, &run.front(), c - c_);
            run.clear();
        }
    }

    if (_modes.get(Mode::SHOW_CURSOR) &&
        _buffer->getHistory() + _cursorRow <
        _buffer->getScroll()  + _buffer->getRows())
    {
        bool     precipitous;       // at the extreme right edge
        uint16_t row;
        uint16_t col;

        row = _cursorRow + (_buffer->getHistory() - _buffer->getScroll());

        ASSERT(row >= 0 && row < _buffer->getRows(), "");

        if (_cursorCol == _buffer->getCols()) {
            col = _cursorCol - 1;
            precipitous = true;
        }
        else {
            col = _cursorCol;
            precipitous = false;
        }

        // Update _damageColBegin and _damageColEnd.
        if (_damageColBegin == _damageColEnd) {
            _damageColBegin = col;
            _damageColEnd   = col + 1;
        }
        else {
            _damageColBegin = std::min(_damageColBegin, col);
            _damageColEnd   = std::max(_damageColEnd,   static_cast<uint16_t>(col + 1));
        }

        // Update _damageRowBegin and _damageRowEnd.
        if (_damageRowBegin == _damageRowEnd) {
            _damageRowBegin = row;
            _damageRowEnd   = row + 1;
        }
        else {
            _damageRowBegin = std::min(_damageRowBegin, row);
            _damageRowEnd   = std::max(_damageRowEnd,   static_cast<uint16_t>(row + 1));
        }

        const Cell & cell   = _buffer->getCell(row, col);
        utf8::Length length = utf8::leadLength(cell.lead());
        run.resize(length);
        std::copy(cell.bytes(), cell.bytes() + length, &run.front());
        run.push_back(NUL);
        _observer.terminalDrawCursor(row, col,
                                     cell.fg(), cell.bg(), cell.attrs(), &run.front(),
                                     precipitous);
    }

    if (true /* show scrollbar? */) {
        _observer.terminalDrawScrollbar(_buffer->getTotalRows(),
                                        _buffer->getScroll(),
                                        _buffer->getRows());
    }
}

void Terminal::write(const uint8_t * data, size_t size) {
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

    _fg         = Cell::defaultFg();
    _bg         = Cell::defaultBg();
    _attrs      = Cell::defaultAttrs();
    _originMode = false;

    _modes.clear();
    _modes.set(Mode::AUTO_WRAP);
    _modes.set(Mode::SHOW_CURSOR);
    _modes.set(Mode::AUTO_REPEAT);
    _modes.set(Mode::ALT_SENDS_ESC);

    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % TAB_SIZE == 0;
    }

    _savedCursorRow  = _cursorRow;
    _savedCursorCol  = _cursorCol;

    _savedFg         = _fg;
    _savedBg         = _bg;
    _savedAttrs      = _attrs;
    _savedOriginMode = _originMode;

    _observer.terminalResetTitle();
}

void Terminal::processRead(const uint8_t * data, size_t size) {
    for (size_t i = 0; i != size; ++i) {
        switch (_utf8Machine.consume(data[i])) {
            case utf8::Machine::State::ACCEPT:
                processChar(_utf8Machine.seq(), _utf8Machine.length());
                break;
            case utf8::Machine::State::REJECT:
                ERROR("Rejecting UTF-8 data.");
                break;
            default:
                break;
        }
    }
}

void Terminal::processChar(utf8::Seq seq, utf8::Length length) {
    _vtMachine.consume(seq, length);

    if (_sync) {        // FIXME too often, may not have been a buffer change.
        fixDamage(0, _buffer->getRows(), 0, _buffer->getCols(), Damage::TTY);
    }
}

void Terminal::machineNormal(utf8::Seq seq, utf8::Length length) throw () {
    seq = translate(seq, length);

    if (_trace) {
        std::cerr << Esc::FG_GREEN << Esc::UNDERLINE << seq << Esc::RESET;
    }

    damageCursor();

    if (_cursorCol == _buffer->getCols()) {
        if (_modes.get(Mode::AUTO_WRAP)) {
            _cursorCol = 0;
            if (_cursorRow == _buffer->getMarginEnd() - 1) {
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

    if (_modes.get(Mode::INSERT)) {
        FATAL("Need to move remainder of line to right.");
    }

    _buffer->setCell(_cursorRow, _cursorCol, Cell::utf8(seq, _attrs, _fg, _bg));
    ++_cursorCol;
}

void Terminal::machineControl(uint8_t c) throw () {
    if (_trace) {
        std::cerr << Esc::FG_YELLOW << Char(c) << Esc::RESET;
    }

    switch (c) {
        case BEL:
            PRINT("BEL!!");
            break;
        case HT:
            tabCursor(TabDir::FORWARD, 1);
            break;
        case BS:
            damageCursor();

            // XXX check this
            if (_cursorCol == 0) {
                if (_modes.get(Mode::AUTO_WRAP)) {
                    if (_cursorRow > _buffer->getMarginBegin()) {
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
            damageCursor();
            _cursorCol = 0;
            break;
        case LF:
            if (_modes.get(Mode::CRLF)) {
                damageCursor();
                _cursorCol = 0;
            }
            // Fall-through:
        case FF:
        case VT:
            if (_cursorRow == _buffer->getMarginEnd() - 1) {
                if (_trace) {
                    std::cerr << "(ADDLINE1)" << std::endl;
                }
                _buffer->addLine();
            }
            else {
                damageCursor();
                ++_cursorRow;
            }

            if (_trace) {
                std::cerr << std::endl;
            }
            break;
        case SO:
            // XXX dubious
            _otherCharSet = true;
            break;
        case SI:
            // XXX dubious
            _otherCharSet = false;
            break;
        case CAN:
        case SUB:
            // XXX reset escape states - assertion currently prevents us getting here
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

void Terminal::machineEscape(uint8_t c) throw () {
    if (_trace) {
        std::cerr << Esc::FG_MAGENTA << "ESC" << Char(c) << Esc::RESET << " ";
    }

    switch (c) {
        case 'D':   // IND - Line Feed (opposite of RI)
            // FIXME
            damageCursor();
            if (_cursorRow == _buffer->getMarginEnd() - 1) {
                if (_trace) {
                    std::cerr << "(ADDLINE1)" << std::endl;
                }
                _buffer->addLine();
            }
            else {
                ++_cursorRow;
            }
            break;
        case 'E':   // NEL - Next Line
            // FIXME
            damageCursor();
            _cursorCol = 0;
            if (_cursorRow == _buffer->getMarginEnd() - 1) {
                if (_trace) {
                    std::cerr << "(ADDLINE1)" << std::endl;
                }
                _buffer->addLine();
            }
            else {
                ++_cursorRow;
            }
            break;
        case 'H':   // HTS - Horizontal Tab Stop
            _tabs[_cursorCol] = true;
            break;
        case 'M':   // RI - Reverse Line Feed (opposite of IND)
            // FIXME dubious
            if (_cursorRow == _buffer->getMarginBegin()) {
                _buffer->insertLines(_buffer->getMarginBegin(), 1);
            }
            else {
                damageCursor();
                --_cursorRow;
            }
            break;
        case 'N':   // SS2 - Set Single Shift 2
            NYI("SS2");
            break;
        case 'O':   // SS3 - Set Single Shift 3
            NYI("SS3");
            break;
        case 'Z':   // DECID - Identify Terminal
            NYI("st.c:2194");
            //ttywrite(VT102ID, sizeof(VT102ID) - 1);
            break;
        case 'c':   // RIS - Reset to initial state
            resetAll();
            break;
        case '=':   // DECKPAM - Keypad Application Mode
            _modes.set(Mode::APPKEYPAD);
            break;
        case '>':   // DECKPNM - Keypad Numeric Mode
            _modes.unset(Mode::APPKEYPAD);
            break;
        case '7':   // DECSC - Save Cursor
            _savedOtherCharSet = _otherCharSet;
            _savedG0           = _G0;
            _savedG1           = _G1;
            _savedCursorRow    = _cursorRow;
            _savedCursorCol    = _cursorCol;
            _savedFg           = _fg;
            _savedBg           = _bg;
            _savedAttrs        = _attrs;
            _savedOriginMode   = _originMode;
            break;
        case '8':   // DECRC - Restore Cursor
            damageCursor();

            _otherCharSet = _savedOtherCharSet;
            _G0           = _savedG0;
            _G1           = _savedG1;
            _cursorRow    = _savedCursorRow;
            _cursorCol    = _savedCursorCol;
            _fg           = _savedFg;
            _bg           = _savedBg;
            _attrs        = _savedAttrs;
            _originMode   = _savedOriginMode;
            break;
        default:
            ERROR("Unknown escape sequence: ESC" << Char(c));
            break;
    }
}

void Terminal::machineCsi(bool priv,
                          const std::vector<int32_t> & args,
                          uint8_t mode) throw () {
    if (_trace) {
        std::cerr << Esc::FG_CYAN << "ESC[";
        if (priv) { std::cerr << '?'; }
        for (auto & a : args) { std::cerr << a << ';'; }
        std::cerr << mode << Esc::RESET << ' ';
    }

    switch (mode) {
        case '@': // ICH - Insert Character
            _buffer->insertCells(_cursorRow, _cursorCol, nthArg(args, 0, 1));
            break;
        case 'A': // CUU - Cursor Up
            moveCursor(_cursorRow - nthArg(args, 0, 1), _cursorCol);
            break;
        case 'B': // CUD - Cursor Down
            moveCursor(_cursorRow + nthArg(args, 0, 1), _cursorCol);
            break;
        case 'C': // CUF - Cursor Forward
            moveCursor(_cursorRow, _cursorCol + nthArg(args, 0, 1));
            break;
        case 'D': // CUB - Cursor Backward
            moveCursor(_cursorRow, _cursorCol - nthArg(args, 0, 1));
            break;
        case 'E': // CNL - Cursor Next Line
            moveCursor(_cursorRow - nthArg(args, 0, 1), 0);
            break;
        case 'F': // CPL - Cursor Preceding Line
            moveCursor(_cursorRow + nthArg(args, 0, 1), 0);
            break;
        case 'G': // CHA - Cursor Horizontal Absolute
            moveCursor(_cursorRow, nthArg(args, 0, 1) - 1);
            break;
        case 'f':       // HVP - Horizontal and Vertical Position
        case 'H':       // CUP - Cursor Position
            if (_trace) { std::cerr << std::endl; }
            if (_originMode) {
                uint16_t row = clamp<int32_t>(nthArg(args, 0, 1) - 1 + _buffer->getMarginBegin(),
                                              _buffer->getMarginBegin(),
                                              _buffer->getMarginEnd() - 1);
                moveCursor(row, nthArg(args, 1, 1) - 1);
            }
            else {
                moveCursor(nthArg(args, 0, 1) - 1, nthArg(args, 1, 1) - 1);
            }
            break;
        case 'I':       // CHT - Cursor Forward Tabulation *
            tabCursor(TabDir::FORWARD, nthArg(args, 0, 1));
            break;
        case 'J': // ED - Erase Data
            // Clear screen.
            switch (nthArg(args, 0)) {
                default:
                case 0: // ED0 - Below
                    for (uint16_t r = _cursorRow + 1; r != _buffer->getRows(); ++r) {
                        _buffer->clearLine(r);
                    }
                    break;
                case 1: // ED1 - Above
                    for (uint16_t r = 0; r != _cursorRow; ++r) {
                        _buffer->clearLine(r);
                    }
                    break;
                case 2: // ED2 - All
                    _buffer->clearAll();
                    _cursorRow = _cursorCol = 0;
                    break;
            }
            break;
        case 'K':   // EL - Erase line
            switch (nthArg(args, 0)) {
                default:
                case 0: // EL0 - Right
                    for (uint16_t c = _cursorCol; c != _buffer->getCols(); ++c) {
                        _buffer->setCell(_cursorRow, c, Cell::blank());
                    }
                    break;
                case 1: // EL1 - Left
                    for (uint16_t c = 0; c != _cursorCol + 1; ++c) {
                        _buffer->setCell(_cursorRow, c, Cell::blank());
                    }
                    break;
                case 2: // EL2 - All
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
        case 'Z':       // CBT - Cursor Backward Tabulation
            tabCursor(TabDir::BACKWARD, nthArg(args, 0, 1));
            break;
        case '`': // HPA
            // TODO damage
            _cursorCol = clamp<uint16_t>(nthArg(args, 0, 1), 1, _buffer->getCols()) - 1;
            break;

        case 'b': // REP
            NYI("REP");
            break;
        case 'c': // Primary DA
            write(reinterpret_cast<const uint8_t *>("\x1B[?6c"), 5);
            break;
        case 'd': // VPA
            _cursorRow = clamp<uint16_t>(nthArg(args, 0, 1), 1, _buffer->getRows()) - 1;
            break;

        case 'g': // TBC
            switch (nthArg(args, 0, 0)) {
                case 0:
                    // "the character tabulation stop at the active presentation"
                    // "position is cleared"
                    if (_cursorCol != _buffer->getCols()) {
                        _tabs[_cursorCol] = false;
                    }
                    break;
                case 1:
                    // "the line tabulation stop at the active line is cleared"
                    NYI("");
                    break;
                case 2:
                    // "all character tabulation stops in the active line are cleared"
                    NYI("");
                    break;
                case 3:
                    // "all character tabulation stops are cleared"
                    std::fill(_tabs.begin(), _tabs.end(), false);
                    break;
                case 4:
                    // "all line tabulation stops are cleared"
                    NYI("");
                    break;
                case 5:
                    // "all tabulation stops are cleared"
                    NYI("");
                    break;
                default:
                    goto default_;
            }
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
                std::vector<int32_t> args2;
                args2.push_back(0);
                processAttributes(args2);
            }
            else {
                processAttributes(args);
            }
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
            // OR IS THIS DECLL0/DECLL1/etc
            NYI("DECSCA");
            break;
        case 'r': // DECSTBM - Set Top and Bottom Margins (scrolling)
            if (priv) {
                goto default_;
            }
            else {
                if (args.empty()) {
                    _buffer->resetMargins();
                    damageCursor();
                    _cursorRow = _cursorCol = 0;
                }
                else {
                    // http://www.vt100.net/docs/vt510-rm/DECSTBM
                    int32_t top    = nthArg(args, 0, 1) - 1;
                    int32_t bottom = nthArg(args, 1, 1) - 1;

                    top    = clamp<int32_t>(top,    0, _buffer->getRows() - 1);
                    bottom = clamp<int32_t>(bottom, 0, _buffer->getRows() - 1);

                    if (bottom > top) {
                        _buffer->setMargins(top, bottom + 1);
                    }
                    else {
                        _buffer->resetMargins();
                    }

                    damageCursor();

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
            damageCursor();
            _cursorRow = _savedCursorRow;
            _cursorCol = _savedCursorCol;
            break;
        case 'y': // DECTST
            NYI("DECTST");
            break;
default_:
        default:
            PRINT("NYI:CSI: ESC  ??? " /*<< Str(seq)*/);
            break;
    }
}

void Terminal::machineDcs(const std::vector<uint8_t> & seq) throw () {
    if (_trace) {
        std::cerr << Esc::FG_RED << "ESC" << Str(seq) << Esc::RESET << ' ';
    }
}

void Terminal::machineOsc(const std::vector<std::string> & args) throw () {
    if (_trace) {
        std::cerr << Esc::FG_MAGENTA << "ESC";
        for (auto & a : args) { std::cerr << a << ';'; }
        std::cerr << Esc::RESET << " ";
    }

    switch (unstringify<int>(nthArg(args, 0))) {
        case 0: // Icon name and window title
        case 1: // Icon label
        case 2: // Window title
            if (args.size() > 1) {      // XXX need if? we have fallback
                _observer.terminalSetTitle(nthArg(args, 1));
            }
            break;
        default:
            PRINT("Unandled: " /*<< Str(seq)*/);
            break;
    }
}

void Terminal::machineSpecial(uint8_t special, uint8_t code) throw () {
    if (_trace) {
        //std::cerr << Esc::FG_BLUE << "ESC" << Str(seq) << Esc::RESET << " ";
    }

    switch (special) {
        case '#':
            switch (code) {
                case '3':   // DECDHL - Double height/width (top half of char)
                    NYI("Double height (top)");
                    break;
                case '4':   // DECDHL - Double height/width (bottom half of char)
                    NYI("Double height (bottom)");
                    break;
                case '5':   // DECSWL - Single height/width
                    break;
                case '6':   // DECDWL - Double width
                    NYI("Double width");
                    break;
                case '8':   // DECALN - Alignment
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
                case '0': // set specg0
                    _G0 = CS_SPECIAL;
                    break;
                case '1': // set altg0
                    NYI("Alternate Character rom");
                    break;
                case '2': // set alt specg0
                    NYI("Alternate Special Character rom");
                    break;
                case 'A': // set ukg0
                    _G0 = CS_UK;
                    break;
                case 'B': // set usg0
                    _G0 = CS_US;
                    break;
                case '<': // Multinational character set
                    NYI("Multinational character set");
                    break;
                case '5': // Finnish
                    NYI("Finnish 1");
                    break;
                case 'C': // Finnish
                    NYI("Finnish 2");
                    break;
                case 'K': // German
                    NYI("German");
                    break;
                default:
                    NYI("Unknown character set: " << code);
                    break;
            }
            break;
        case ')':
            switch (code) {
                case '0': // set specg1
                    PRINT("\nG1 = SPECIAL");
                    _G1 = CS_SPECIAL;
                    break;
                case '1': // set altg1
                    NYI("Alternate Character rom");
                    break;
                case '2': // set alt specg1
                    NYI("Alternate Special Character rom");
                    break;
                case 'A': // set ukg0
                    PRINT("\nG1 = UK");
                    _G1 = CS_UK;
                    break;
                case 'B': // set usg0
                    PRINT("\nG1 = US");
                    _G1 = CS_US;
                    break;
                case '<': // Multinational character set
                    NYI("Multinational character set");
                    break;
                case '5': // Finnish
                    NYI("Finnish 1");
                    break;
                case 'C': // Finnish
                    NYI("Finnish 2");
                    break;
                case 'K': // German
                    NYI("German");
                    break;
                default:
                    NYI("Unknown character set: " << code);
                    break;
            }
            break;
        default:
            NYI("Special: " /*<< Str(seq)*/);
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
            case 2: // Faint (low/decreased intensity)
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
                case 1: // DECCKM - Cursor Keys Mode - Application / Cursor
                    _modes.setTo(Mode::APPCURSOR, set);
                    break;
                case 2: // DECANM - ANSI/VT52 Mode
                    NYI("DECANM: " << set);
                    _otherCharSet = false;  // XXX dubious
                    _G0           = CS_US;
                    _G1           = CS_US;
                    break;
                case 3: // DECCOLM - Column Mode
                    if (set) {
                        // resize 132x24
                        _observer.terminalResize(24, 132);
                    }
                    else {
                        // resize 80x24
                        _observer.terminalResize(24, 80);
                    }
                    break;
                case 4: // DECSCLM - Scroll Mode - Smooth / Jump (IGNORED)
                    NYI("DECSCLM: " << set);
                    break;
                case 5: // DECSCNM - Screen Mode - Reverse / Normal
                    if (_modes.get(Mode::REVERSE) != set) {
                        _modes.setTo(Mode::REVERSE, set);
                        _buffer->damageAll();
                    }
                    break;
                case 6: // DECOM - Origin Mode - Relative / Absolute
                    _originMode = set;

                    damageCursor();

                    if (_originMode) {
                        _cursorRow = _buffer->getMarginBegin();
                    }
                    else {
                        _cursorRow = 0;
                    }

                    _cursorCol = 0;
                    break;
                case 7: // DECAWM - Auto Wrap Mode
                    _modes.setTo(Mode::AUTO_WRAP, set);
                    break;
                case 8: // DECARM - Auto Repeat Mode
                    _modes.setTo(Mode::AUTO_REPEAT, set);
                    break;
                case 9: // DECINLM - Interlacing Mode
                    NYI("DECINLM");
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
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damage::SCROLL);
                    if (a != 1049) {
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
                        damageCursor();

                        _cursorRow  = _savedCursorRow;
                        _cursorCol  = _savedCursorCol;
                        _fg         = _savedFg;
                        _bg         = _savedBg;
                        _attrs      = _savedAttrs;
                        _originMode = _savedOriginMode;
                    }
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
                    _modes.setTo(Mode::ECHO, set);      // XXX correct sense
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
