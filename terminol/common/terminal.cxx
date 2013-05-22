// vi:noai:sw=4

#include "terminol/common/terminal.hxx"
#include "terminol/support/time.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/escape.hxx"

#include <algorithm>
#include <numeric>

namespace {

int32_t nthArg(const std::vector<int32_t> & args, size_t n, int32_t fallback = 0) {
    if (n < args.size()) {
        return args[n];
    }
    else {
        return fallback;
    }
}

// Same as nth arg, but use fallback if arg is zero.
int32_t nthArgNonZero(const std::vector<int32_t> & args, size_t n, int32_t fallback) {
    int32_t arg = nthArg(args, n, fallback);
    return arg != 0 ? arg : fallback;
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
                   const Config & config,
                   uint16_t       rows,
                   uint16_t       cols,
                   const KeyMap & keyMap,
                   I_Tty        & tty) :
    _observer(observer),
    _dispatch(false),
    //
    _config(config),
    _keyMap(keyMap),
    //
    _priBuffer(_config, rows, cols,
               _config.getUnlimitedScrollBack() ?
               std::numeric_limits<size_t>::max() :
               _config.getScrollBackHistory()),
    _altBuffer(_config, rows, cols, 0),
    _buffer(&_priBuffer),
    //
    _modes(),
    _tabs(_buffer->getCols()),
    //
    _cursor(),
    _savedCursor(),
    //
    _damage(),
    //
    _tty(tty),
    _dumpWrites(false),
    _writeBuffer(),
    _utf8Machine(),
    _vtMachine(*this)
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

    int16_t priRowAdjust = _priBuffer.resize(rows, cols);
    int16_t altRowAdjust = _altBuffer.resize(rows, cols);
    int16_t rowAdjust    = _buffer == &_priBuffer ? priRowAdjust : altRowAdjust;

    _cursor.row = clamp< int16_t>(_cursor.row + rowAdjust, 0, rows - 1);
    _cursor.col = clamp<uint16_t>(_cursor.col,             0, cols - 1);

    // XXX is this correct for the saved cursor row/col?
    _savedCursor.row = std::min<uint16_t>(_savedCursor.row, rows - 1);
    _savedCursor.col = std::min<uint16_t>(_savedCursor.col, cols - 1);

    _tabs.resize(cols);
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % TAB_SIZE == 0;
    }
}

void Terminal::redraw(uint16_t rowBegin, uint16_t rowEnd,
                      uint16_t colBegin, uint16_t colEnd) {
    fixDamage(rowBegin, rowEnd, colBegin, colEnd, Damager::EXPOSURE);
}

void Terminal::keyPress(xkb_keysym_t keySym, uint8_t state) {
    if (!handleKeyBinding(keySym, state) && _keyMap.isPotent(keySym)) {
        if (_config.getScrollOnTtyKeyPress() && _buffer->scrollBottom()) {
            fixDamage(0, _buffer->getRows(),
                      0, _buffer->getCols(),
                      Damager::SCROLL);
        }

        std::vector<uint8_t> str;
        if (_keyMap.convert(keySym, state,
                            _modes.get(Mode::APPKEYPAD),
                            _modes.get(Mode::APPCURSOR),
                            _modes.get(Mode::CR_ON_LF),
                            _modes.get(Mode::DELETE_SENDS_DEL),
                            _modes.get(Mode::ALT_SENDS_ESC),
                            str)) {
            write(&str.front(), str.size());
        }
    }
}

void Terminal::buttonPress(Button button, uint8_t state,
                           int count, bool within, uint16_t row, uint16_t col) {
    PRINT("press: " << button << ", count=" << count <<
          ", within=" << within << ", " << row << 'x' << col);

    if (button == Button::LEFT) {
    }
    else if (button == Button::MIDDLE) {
        _observer.terminalPaste(false);
    }
}

void Terminal::motionNotify(bool within, uint16_t row, uint16_t col) {
    //PRINT("motion: within=" << within << ", " << row << 'x' << col);
}

void Terminal::buttonRelease(bool broken) {
    //PRINT("release, grabbed=" << grabbed);
}

void Terminal::scrollWheel(ScrollDir dir, uint8_t state) {
    switch (dir) {
        case ScrollDir::UP:
            // TODO consolidate scroll operations with method.
            if (_buffer->scrollUp(std::max(1, _buffer->getRows() / 4))) {
                fixDamage(0, _buffer->getRows(),
                          0, _buffer->getCols(),
                          Damager::SCROLL);
            }
            break;
        case ScrollDir::DOWN:
            if (_buffer->scrollDown(std::max(1, _buffer->getRows() / 4))) {
                fixDamage(0, _buffer->getRows(),
                          0, _buffer->getCols(),
                          Damager::SCROLL);
            }
            break;
    }
}

void Terminal::paste(const uint8_t * data, size_t size) {
    write(data, size);
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
            size_t rval = _tty.read(buf, _config.getSyncTty() ? 16 : sizeof buf);
            if (rval == 0) { /*PRINT("Would block");*/ break; }
            //PRINT("Read: " << rval);
            processRead(buf, rval);
        } while (!timer.expired());

        //PRINT("Exit loop");
    }
    catch (const I_Tty::Exited & ex) {
        _observer.terminalChildExited(ex.exitCode);
    }

    if (!_config.getSyncTty()) {
        fixDamage(0, _buffer->getRows(), 0, _buffer->getCols(), Damager::TTY);
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
        do {
            size_t rval = _tty.write(&_writeBuffer.front(), _writeBuffer.size());
            if (rval == 0) { break; }
            _writeBuffer.erase(_writeBuffer.begin(), _writeBuffer.begin() + rval);
        } while (!_writeBuffer.empty());
    }
    catch (const I_Tty::Error & ex) {
        _dumpWrites = true;
        _writeBuffer.clear();
    }
}

bool Terminal::handleKeyBinding(xkb_keysym_t keySym, uint8_t state) {
    // FIXME no hard-coded keybindings. Use config.

    if (state & _keyMap.maskShift() &&
        state & _keyMap.maskControl())
    {
        switch (keySym) {
            case XKB_KEY_X:
                _observer.terminalCopy("PRIMARY SELECTION", false);
                return true;
            case XKB_KEY_C:
                _observer.terminalCopy("CLIPBOARD SELECTION", true);
                return true;
            case XKB_KEY_V: {
                std::string text;
                _observer.terminalPaste(true);
                return true;
            }
        }
    }

    if (state & _keyMap.maskShift()) {
        switch (keySym) {
            case XKB_KEY_Up:
                if (_buffer->scrollUp(1)) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damager::SCROLL);
                }
                return true;
            case XKB_KEY_Down:
                if (_buffer->scrollDown(1)) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damager::SCROLL);
                }
                return true;
            case XKB_KEY_Page_Up:
                if (_buffer->scrollUp(_buffer->getRows())) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damager::SCROLL);
                }
                return true;
            case XKB_KEY_Page_Down:
                if (_buffer->scrollDown(_buffer->getRows())) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damager::SCROLL);
                }
                return true;
            case XKB_KEY_Home:
                if (_buffer->scrollTop()) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damager::SCROLL);
                }
                return true;
            case XKB_KEY_End:
                if (_buffer->scrollBottom()) {
                    fixDamage(0, _buffer->getRows(),
                              0, _buffer->getCols(),
                              Damager::SCROLL);
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

void Terminal::moveCursorOriginMode(int32_t row, int32_t col) {
    moveCursor(row + (_cursor.originMode ? _buffer->getMarginBegin() : 0), col);
}

void Terminal::moveCursor(int32_t row, int32_t col) {
    damageCursor();

    if (_cursor.originMode) {
        _cursor.row = clamp<int32_t>(row,
                                    _buffer->getMarginBegin(),
                                    _buffer->getMarginEnd() - 1);
    }
    else {
        _cursor.row = clamp<int32_t>(row, 0, _buffer->getRows() - 1);
    }

    _cursor.col      = clamp<int32_t>(col, 0, _buffer->getCols() - 1);
    _cursor.wrapNext = false;
}

void Terminal::tabCursor(TabDir dir, uint16_t count) {
    damageCursor();

    switch (dir) {
        case TabDir::FORWARD:
            while (count != 0) {
                ++_cursor.col;

                if (_cursor.col == _buffer->getCols()) {
                    --_cursor.col;
                    break;
                }

                if (_tabs[_cursor.col]) {
                    --count;
                }
            }
            break;
        case TabDir::BACKWARD:
            while (count != 0) {
                if (_cursor.col == 0) {
                    break;
                }

                --_cursor.col;

                if (_tabs[_cursor.col]) {
                    --count;
                }
            }
            break;
    }
}

void Terminal::damageCursor() {
    ASSERT(_cursor.row < _buffer->getRows(), "");
    ASSERT(_cursor.col < _buffer->getCols(), "");
    _buffer->damageCell(_cursor.row, _cursor.col);
}

void Terminal::fixDamage(uint16_t rowBegin, uint16_t rowEnd,
                         uint16_t colBegin, uint16_t colEnd,
                         Damager damage) {
    if (damage == Damager::TTY &&
        _config.getScrollOnTtyOutput() &&
        _buffer->scrollBottom())
    {
        // Promote the damage from TTY to SCROLL.
        damage = Damager::SCROLL;
    }

    if (_observer.terminalFixDamageBegin(damage != Damager::EXPOSURE)) {
        draw(rowBegin, rowEnd, colBegin, colEnd, damage);

        bool scrollbar =    // Identical in two places.
            damage == Damager::SCROLL || damage == Damager::EXPOSURE ||
            (damage == Damager::TTY && _buffer->getBarDamage());

        _observer.terminalFixDamageEnd(damage != Damager::EXPOSURE,
                                       _damage.rowBegin, _damage.rowEnd,
                                       _damage.colBegin, _damage.colEnd,
                                       scrollbar);

        if (damage == Damager::TTY) {
            _buffer->resetDamage();
        }
    }
    else {
        // If we received a redraw() then the observer had better be able
        // to handle it.
        ENFORCE(damage != Damager::EXPOSURE, "");
    }
}

utf8::Seq Terminal::translate(utf8::Seq seq, utf8::Length length) const {
    if (length == utf8::Length::L1) {
        uint8_t ascii = seq.bytes[0];

        for (const CharSub * cs = _cursor.otherCharSet ? _cursor.G1 : _cursor.G0;
             cs->match != NUL; ++cs)
        {
            if (ascii == cs->match) {
                if (_config.getTraceTty()) {
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
                    Damager damage) {
    _damage.reset();

    // Declare run at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<uint8_t> run;
    // Reserve the largest amount of memory we could require.
    run.reserve(_buffer->getCols() * utf8::Length::LMAX + 1);

    for (uint16_t r = rowBegin; r != rowEnd; ++r) {
        uint16_t     c_ = 0;    // Accumulation start column.
        uint16_t     fg = 0, bg = 0;
        AttributeSet attrs;
        uint16_t     c;
        uint16_t     colBegin2, colEnd2;

        if (damage == Damager::TTY) {
            _buffer->getDamage(r, colBegin2, colEnd2);
            //PRINT("Consulted buffer damage, got: " << colBegin2 << ", " << colEnd2);
        }
        else {
            colBegin2 = colBegin;
            colEnd2   = colEnd;
            //PRINT("External damage damage, got: " << colBegin2 << ", " << colEnd2);
        }

        // Update _damage.colBegin and _damage.colEnd.
        if (_damage.colBegin == _damage.colEnd) {
            _damage.colBegin = colBegin2;
            _damage.colEnd   = colEnd2;
        }
        else if (colBegin2 != colEnd2) {
            _damage.colBegin = std::min(_damage.colBegin, colBegin2);
            _damage.colEnd   = std::max(_damage.colEnd,   colEnd2);
        }

        // Update _damage.rowBegin and _damage.rowEnd.
        if (colBegin2 != colEnd2) {
            if (_damage.rowBegin == _damage.rowEnd) {
                _damage.rowBegin = r;
            }

            _damage.rowEnd = r + 1;
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

                if (_modes.get(Mode::REVERSE)) { std::swap(fg, bg); }

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
        _buffer->getHistory() + _cursor.row <
        _buffer->getScroll()  + _buffer->getRows())
    {
        uint16_t row;
        uint16_t col;

        row = _cursor.row + (_buffer->getHistory() - _buffer->getScroll());
        ASSERT(row < _buffer->getRows(), "");

        col = _cursor.col;
        ASSERT(col < _buffer->getCols(), "");

        // Update _damage.colBegin and _damage.colEnd.
        if (_damage.colBegin == _damage.colEnd) {
            _damage.colBegin = col;
            _damage.colEnd   = col + 1;
        }
        else {
            _damage.colBegin = std::min(_damage.colBegin, col);
            _damage.colEnd   = std::max(_damage.colEnd,   static_cast<uint16_t>(col + 1));
        }

        // Update _damage.rowBegin and _damage.rowEnd.
        if (_damage.rowBegin == _damage.rowEnd) {
            _damage.rowBegin = row;
            _damage.rowEnd   = row + 1;
        }
        else {
            _damage.rowBegin = std::min(_damage.rowBegin, row);
            _damage.rowEnd   = std::max(_damage.rowEnd,   static_cast<uint16_t>(row + 1));
        }

        const Cell & cell   = _buffer->getCell(row, col);
        utf8::Length length = utf8::leadLength(cell.lead());
        run.resize(length);
        std::copy(cell.bytes(), cell.bytes() + length, &run.front());
        run.push_back(NUL);
        _observer.terminalDrawCursor(row, col,
                                     cell.fg(), cell.bg(), cell.attrs(), &run.front(),
                                     _cursor.wrapNext);
    }

    bool scrollbar =    // Identical in two places.
        damage == Damager::SCROLL || damage == Damager::EXPOSURE ||
        (damage == Damager::TTY && _buffer->getBarDamage());

    if (scrollbar) {
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
        catch (const I_Tty::Error &) {
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
    _buffer->clear();

    _modes.clear();
    _modes.set(Mode::AUTO_WRAP);
    _modes.set(Mode::SHOW_CURSOR);
    _modes.set(Mode::AUTO_REPEAT);
    _modes.set(Mode::ALT_SENDS_ESC);

    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % TAB_SIZE == 0;
    }

    _cursor.reset();
    _savedCursor.reset();

    _observer.terminalResetTitle();
}

void Terminal::processRead(const uint8_t * data, size_t size) {
    for (size_t i = 0; i != size; ++i) {
        switch (_utf8Machine.consume(data[i])) {
            case utf8::Machine::State::ACCEPT:
                processChar(_utf8Machine.seq(), _utf8Machine.length());
                ASSERT(_cursor.row < _buffer->getRows(), "");
                ASSERT(_cursor.col < _buffer->getCols(), "");
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

    if (_config.getSyncTty()) {        // FIXME too often, may not have been a buffer change.
        fixDamage(0, _buffer->getRows(), 0, _buffer->getCols(), Damager::TTY);
    }
}

void Terminal::machineNormal(utf8::Seq seq, utf8::Length length) throw () {
    seq = translate(seq, length);

    if (_config.getTraceTty()) {
        std::cerr << Esc::FG_GREEN << Esc::UNDERLINE << seq << Esc::RESET;
    }

    if (_cursor.wrapNext && _modes.get(Mode::AUTO_WRAP)) {
        moveCursor(_cursor.row, 0);

        if (_cursor.row == _buffer->getMarginEnd() - 1) {
            _buffer->addLine();
        }
        else {
            moveCursor(_cursor.row + 1, _cursor.col);
        }
    }

    ASSERT(_cursor.col < _buffer->getCols(), "");
    ASSERT(_cursor.row < _buffer->getRows(), "");

    if (_modes.get(Mode::INSERT)) {
        _buffer->insertCells(_cursor.row, _cursor.col, 1);
    }

    _buffer->setCell(_cursor.row, _cursor.col,
                     Cell::utf8(seq, _cursor.attrs, _cursor.fg, _cursor.bg));

    if (_cursor.col == _buffer->getCols() - 1) {
        _cursor.wrapNext = true;
    }
    else {
        moveCursor(_cursor.row, _cursor.col + 1);
    }

    ASSERT(_cursor.col < _buffer->getCols(), "");
}

void Terminal::machineControl(uint8_t c) throw () {
    if (_config.getTraceTty()) {
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

            if (_cursor.wrapNext) {
                _cursor.wrapNext = false;
            }
            else {
                if (_cursor.col == 0) {
                    if (_modes.get(Mode::AUTO_WRAP)) {
                        if (_cursor.row > _buffer->getMarginBegin()) {
                            _cursor.col = _buffer->getCols() - 1;
                            --_cursor.row;
                        }
                    }
                }
                else {
                    --_cursor.col;
                }
            }
            break;
        case CR:
            damageCursor();
            _cursor.col = 0;
            break;
        case LF:
            if (_modes.get(Mode::CR_ON_LF)) {
                damageCursor();
                _cursor.col = 0;
            }
            // Fall-through:
        case FF:
        case VT:
            if (_cursor.row == _buffer->getMarginEnd() - 1) {
                if (_config.getTraceTty()) {
                    std::cerr << "(ADDLINE1)" << std::endl;
                }
                _buffer->addLine();
            }
            else {
                moveCursor(_cursor.row + 1, _cursor.col);
            }

            if (_config.getTraceTty()) {
                std::cerr << std::endl;
            }
            break;
        case SO:
            // XXX dubious
            _cursor.otherCharSet = true;
            break;
        case SI:
            // XXX dubious
            _cursor.otherCharSet = false;
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
    if (_config.getTraceTty()) {
        std::cerr << Esc::FG_MAGENTA << "ESC" << Char(c) << Esc::RESET << " ";
    }

    switch (c) {
        case 'D':   // IND - Line Feed (opposite of RI)
            // FIXME still dubious
            if (_cursor.row == _buffer->getMarginEnd() - 1) {
                if (_config.getTraceTty()) { std::cerr << "(ADDLINE2)" << std::endl; }
                _buffer->addLine();
            }
            else {
                moveCursor(_cursor.row + 1, _cursor.col);
            }
            break;
        case 'E':   // NEL - Next Line
            // FIXME still dubious
            moveCursor(_cursor.row, 0);
            if (_cursor.row == _buffer->getMarginEnd() - 1) {
                if (_config.getTraceTty()) { std::cerr << "(ADDLINE3)" << std::endl; }
                _buffer->addLine();
            }
            else {
                moveCursor(_cursor.row + 1, _cursor.col);
            }
            break;
        case 'H':   // HTS - Horizontal Tab Stop
            _tabs[_cursor.col] = true;
            break;
        case 'M':   // RI - Reverse Line Feed (opposite of IND)
            // FIXME still dubious
            if (_cursor.row == _buffer->getMarginBegin()) {
                _buffer->insertLines(_buffer->getMarginBegin(), 1);
            }
            else {
                moveCursor(_cursor.row - 1, _cursor.col);
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
            _savedCursor = _cursor;
            break;
        case '8':   // DECRC - Restore Cursor
            damageCursor();
            _cursor = _savedCursor;
            break;
        default:
            ERROR("Unknown escape sequence: ESC" << Char(c));
            break;
    }
}

void Terminal::machineCsi(bool priv,
                          const std::vector<int32_t> & args,
                          uint8_t mode) throw () {
    if (_config.getTraceTty()) {
        std::cerr << Esc::FG_CYAN << "ESC[";
        if (priv) { std::cerr << '?'; }
        bool first = true;
        for (auto & a : args) {
            if (first) { first = false; }
            else       { std::cerr << ';'; }
            std::cerr << a;
        }
        std::cerr << mode << Esc::RESET << ' ';
    }

    switch (mode) {
        case '@': { // ICH - Insert Character
            // XXX what about _cursor.wrapNext
            int32_t count = nthArgNonZero(args, 0, 1);
            count = clamp(count, 1, _buffer->getCols() - _cursor.col);
            _buffer->insertCells(_cursor.row, _cursor.col, count);
            break;
        }
        case 'A': // CUU - Cursor Up
            moveCursor(_cursor.row - nthArgNonZero(args, 0, 1), _cursor.col);
            break;
        case 'B': // CUD - Cursor Down
            moveCursor(_cursor.row + nthArgNonZero(args, 0, 1), _cursor.col);
            break;
        case 'C': // CUF - Cursor Forward
            moveCursor(_cursor.row, _cursor.col + nthArgNonZero(args, 0, 1));
            break;
        case 'D': // CUB - Cursor Backward
            moveCursor(_cursor.row, _cursor.col - nthArgNonZero(args, 0, 1));
            break;
        case 'E': // CNL - Cursor Next Line
            moveCursor(_cursor.row + nthArgNonZero(args, 0, 1), 0);
            break;
        case 'F': // CPL - Cursor Preceding Line
            moveCursor(_cursor.row - nthArgNonZero(args, 0, 1), 0);
            break;
        case 'G': // CHA - Cursor Horizontal Absolute
            moveCursor(_cursor.row, nthArgNonZero(args, 0, 1) - 1);
            break;
        case 'f':       // HVP - Horizontal and Vertical Position
        case 'H':       // CUP - Cursor Position
            if (_config.getTraceTty()) { std::cerr << std::endl; }
            moveCursorOriginMode(nthArg(args, 0, 1) - 1, nthArg(args, 1, 1) - 1);
            break;
        case 'I':       // CHT - Cursor Forward Tabulation *
            tabCursor(TabDir::FORWARD, nthArg(args, 0, 1));
            break;
        case 'J': // ED - Erase Data
            // Clear screen.
            switch (nthArg(args, 0)) {
                default:
                case 0: // ED0 - Below
                    _buffer->clearLineRight(_cursor.row, _cursor.col);
                    _buffer->clearBelow(_cursor.row + 1);
                    break;
                case 1: // ED1 - Above
                    _buffer->clearAbove(_cursor.row);
                    _buffer->clearLineLeft(_cursor.row, _cursor.col + 1);
                    break;
                case 2: // ED2 - All
                    _buffer->clear();
                    _cursor.row = _cursor.col = 0;
                    break;
            }
            break;
        case 'K':   // EL - Erase line
            switch (nthArg(args, 0)) {
                default:
                case 0: // EL0 - Right (inclusive of cursor position)
                    _buffer->clearLineRight(_cursor.row, _cursor.col);
                    break;
                case 1: // EL1 - Left (inclusive of cursor position)
                    _buffer->clearLineLeft(_cursor.row, _cursor.col + 1);
                    break;
                case 2: // EL2 - All
                    _buffer->clearLine(_cursor.row);
                    break;
            }
            break;
        case 'L': // IL - Insert Lines
            if (_cursor.row >= _buffer->getMarginBegin() &&
                _cursor.row <  _buffer->getMarginEnd())
            {
                int32_t count = nthArgNonZero(args, 0, 1);
                count = clamp(count, 1, _buffer->getMarginEnd() - _cursor.row);
                _buffer->insertLines(_cursor.row, count);
            }
            break;
        case 'M': // DL - Delete Lines
            if (_cursor.row >= _buffer->getMarginBegin() &&
                _cursor.row <  _buffer->getMarginEnd())
            {
                int32_t count = nthArgNonZero(args, 0, 1);
                count = clamp(count, 1, _buffer->getMarginEnd() - _cursor.row);
                _buffer->eraseLines(_cursor.row, count);
            }
            break;
        case 'P': { // DCH - Delete Character
            // FIXME what about wrap-next?
            int32_t count = nthArgNonZero(args, 0, 1);
            count = clamp(count, 1, _buffer->getCols() - _cursor.col);
            _buffer->eraseCells(_cursor.row, _cursor.col, count);
            break;
        }
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
            tabCursor(TabDir::BACKWARD, nthArgNonZero(args, 0, 1));
            break;
        case '`': // HPA
            moveCursor(_cursor.row, nthArgNonZero(args, 0, 1) - 1);
            break;
        case 'b': // REP
            NYI("REP");
            break;
        case 'c': // Primary DA
            write(reinterpret_cast<const uint8_t *>("\x1B[?6c"), 5);
            break;
        case 'd': // VPA - Vertical Position Absolute
            moveCursorOriginMode(nthArg(args, 0, 1) - 1, _cursor.col);
            break;
        case 'g': // TBC
            switch (nthArg(args, 0, 0)) {
                case 0:
                    // "the character tabulation stop at the active presentation"
                    // "position is cleared"
                    _tabs[_cursor.col] = false;
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
            else {
                switch (nthArg(args, 0)) {
                    case 5: {
                        // QDS - Query Device Status
                        // RDO - Report Device OK: <ESC>[0n
                        std::ostringstream ost;
                        ost << ESC << "[0n";
                        const std::string & str = ost.str();
                        _writeBuffer.insert(_writeBuffer.begin(), str.begin(), str.end());
                        break;
                    }
                    case 6: {
                        // QCP - Query Cursor Position
                        // RCP - Report Cursor Position

                        // XXX Is cursor position reported absolute irrespective of
                        // origin-mode.

                        std::ostringstream ost;
                        ost << ESC << '[' << _cursor.row + 1 << ';' << _cursor.col + 1 << 'R';
                        const std::string & str = ost.str();
                        _writeBuffer.insert(_writeBuffer.begin(), str.begin(), str.end());
                    }
                    case 7: {
                        // Ps = 7   Request Display Name
                        NYI("");
                        break;
                    }
                    case 8: {
                        // Ps = 8   Request Version Number (place in window title)
                        NYI("");
                        break;
                    }
                    default:
                        NYI("");
                        break;
                }
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
                    moveCursorOriginMode(0, 0);
                }
                else {
                    // http://www.vt100.net/docs/vt510-rm/DECSTBM
                    int32_t top    = nthArgNonZero(args, 0, 1) - 1;
                    int32_t bottom = nthArgNonZero(args, 1, _cursor.row + 1) - 1;

                    top    = clamp<int32_t>(top,    0, _buffer->getRows() - 1);
                    bottom = clamp<int32_t>(bottom, 0, _buffer->getRows() - 1);

                    if (bottom > top) {
                        _buffer->setMargins(top, bottom + 1);
                    }
                    else {
                        _buffer->resetMargins();
                    }

                    moveCursorOriginMode(0, 0);
                }
            }
            break;
        case 's': // save cursor
            _savedCursor.row = _cursor.row;
            _savedCursor.col = _cursor.col;
            break;
        case 't': // window ops?
            // FIXME see 'Window Operations' in man 7 urxvt.
            NYI("Window ops");
            break;
        case 'u': // restore cursor
            moveCursor(_savedCursor.row, _savedCursor.col);
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
    if (_config.getTraceTty()) {
        std::cerr << Esc::FG_RED << "ESC" << Str(seq) << Esc::RESET << ' ';
    }
}

void Terminal::machineOsc(const std::vector<std::string> & args) throw () {
    if (_config.getTraceTty()) {
        std::cerr << Esc::FG_MAGENTA << "ESC";
        for (auto & a : args) { std::cerr << a << ';'; }
        std::cerr << Esc::RESET << " ";
    }

    if (!args.empty()) {
        switch (unstringify<int>(args[0])) {
            case 0: // Icon name and window title
            case 1: // Icon label
            case 2: // Window title
                if (args.size() > 1) {
                    _observer.terminalSetTitle(args[1]);
                }
                break;
            case 55:
                NYI("Log history to file");
                break;
            default:
                // TODO consult http://rtfm.etla.org/xterm/ctlseq.html AND man 7 urxvt.
                PRINT("Unandled: OSC");
                for (const auto & a : args) {
                    PRINT(a);
                }
                break;
        }
    }
}

void Terminal::machineSpecial(uint8_t special, uint8_t code) throw () {
    if (_config.getTraceTty()) {
        std::cerr << Esc::FG_BLUE << "ESC" << Char(special) << Char(code) << Esc::RESET << " ";
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
                case '8': { // DECALN - Alignment
                    // Fill terminal with 'E'
                    Cell cell = Cell::utf8(utf8::Seq('E'),
                                           _cursor.attrs, _cursor.fg, _cursor.bg);
                    for (uint16_t r = 0; r != _buffer->getRows(); ++r) {
                        for (uint16_t c = 0; c != _buffer->getCols(); ++c) {
                            _buffer->setCell(r, c, cell);
                        }
                    }
                    break;
                }
                default:
                    NYI("?");
                    break;
            }
            break;
        case '(':
            switch (code) {
                case '0': // set specg0
                    _cursor.G0 = CS_SPECIAL;
                    break;
                case '1': // set altg0
                    NYI("Alternate Character rom");
                    break;
                case '2': // set alt specg0
                    NYI("Alternate Special Character rom");
                    break;
                case 'A': // set ukg0
                    _cursor.G0 = CS_UK;
                    break;
                case 'B': // set usg0
                    _cursor.G0 = CS_US;
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
                    _cursor.G1 = CS_SPECIAL;
                    break;
                case '1': // set altg1
                    NYI("Alternate Character rom");
                    break;
                case '2': // set alt specg1
                    NYI("Alternate Special Character rom");
                    break;
                case 'A': // set ukg0
                    PRINT("\nG1 = UK");
                    _cursor.G1 = CS_UK;
                    break;
                case 'B': // set usg0
                    PRINT("\nG1 = US");
                    _cursor.G1 = CS_US;
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

    // FIXME check man 7 urxvt:

    // FIXME is it right to loop?
    for (size_t i = 0; i != args.size(); ++i) {
        int32_t v = args[i];

        switch (v) {
            case 0: // Reset/Normal
                _cursor.bg    = Cell::defaultBg();
                _cursor.fg    = Cell::defaultFg();
                _cursor.attrs = Cell::defaultAttrs();
                break;
            case 1: // Bold
                _cursor.attrs.set(Attribute::BOLD);
                // Normal -> Bright.
                if (_cursor.fg < 8) { _cursor.fg += 8; }
                break;
            case 2: // Faint (low/decreased intensity)
                _cursor.attrs.unset(Attribute::BOLD);
                // Bright -> Normal.
                if (_cursor.fg >= 8 && _cursor.fg < 16) { _cursor.fg -= 8; }
                break;
            case 3: // Italic: on
                _cursor.attrs.set(Attribute::ITALIC);
                break;
            case 4: // Underline: Single
                _cursor.attrs.set(Attribute::UNDERLINE);
                break;
            case 5: // Blink: slow
            case 6: // Blink: rapid
                _cursor.attrs.set(Attribute::BLINK);
                break;
            case 7: // Image: Negative
                _cursor.attrs.set(Attribute::INVERSE);
                break;
            case 8: // Conceal (not widely supported)
                _cursor.attrs.set(Attribute::CONCEAL);
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
                _cursor.attrs.unset(Attribute::BOLD);
                // Bright -> Normal.
                if (_cursor.fg >= 8 && _cursor.fg < 16) { _cursor.fg -= 8; }
                break;
            case 22: // Normal color or intensity (neither bold nor faint)
                _cursor.attrs.unset(Attribute::BOLD);
                // Bright -> Normal.        XXX is this right?
                if (_cursor.fg >= 8 && _cursor.fg < 16) { _cursor.fg -= 8; }
                break;
            case 23: // Not italic, not Fraktur
                _cursor.attrs.unset(Attribute::ITALIC);
                break;
            case 24: // Underline: None (not singly or doubly underlined)
                _cursor.attrs.unset(Attribute::UNDERLINE);
                break;
            case 25: // Blink: off
                _cursor.attrs.unset(Attribute::BLINK);
                break;
            case 26: // Reserved?
                _cursor.attrs.set(Attribute::INVERSE);
                break;
            case 27: // Image: Positive
                _cursor.attrs.unset(Attribute::INVERSE);
                break;
            case 28: // Reveal (conceal off - not widely supported)
                _cursor.attrs.unset(Attribute::CONCEAL);
                break;
            case 29: // Not crossed-out (not widely supported)
                NYI("Crossed-out");
                break;
                // 30..37 (set foreground colour - handled separately)
            case 38:
                // https://github.com/robertknight/konsole/blob/master/user-doc/README.moreColors
                if (i + 1 < args.size()) {
                    i += 1;
                    switch (args[i]) {
                        case 0:
                            NYI("User defined foreground");
                            break;
                        case 1:
                            NYI("Transparent foreground");
                            break;
                        case 2:
                            if (i + 3 < args.size()) {
                                // 24-bit foreground support
                                // ESC[ … 48;2;<r>;<g>;<b> … m Select RGB foreground color
                                NYI("24-bit RGB foreground");
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 3:
                            if (i + 3 < args.size()) {
                                NYI("24-bit CMY foreground");
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 4:
                            if (i + 4 < args.size()) {
                                NYI("24-bit CMYK foreground");
                                i += 4;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 5:
                            if (i + 1 < args.size()) {
                                i += 1;
                                int32_t v2 = args[i];
                                if (v2 >= 0 && v2 < 256) {
                                    _cursor.fg = v2;
                                }
                                else {
                                    ERROR("Colour out of range: " << v2);
                                }
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        default:
                            NYI("Unknown?");
                    }
                }
                break;
            case 39:
                _cursor.fg = Cell::defaultFg();
                break;
                // 40..47 (set background colour - handled separately)
            case 48:
                // https://github.com/robertknight/konsole/blob/master/user-doc/README.moreColors
                if (i + 1 < args.size()) {
                    i += 1;
                    switch (args[i]) {
                        case 0:
                            NYI("User defined background");
                            break;
                        case 1:
                            NYI("Transparent background");
                            break;
                        case 2:
                            if (i + 3 < args.size()) {
                                // 24-bit background support
                                // ESC[ … 48;2;<r>;<g>;<b> … m Select RGB background color
                                NYI("24-bit RGB background");
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 3:
                            if (i + 3 < args.size()) {
                                NYI("24-bit CMY background");
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 4:
                            if (i + 4 < args.size()) {
                                NYI("24-bit CMYK background");
                                i += 4;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 5:
                            if (i + 1 < args.size()) {
                                i += 1;
                                int32_t v2 = args[i];
                                if (v2 >= 0 && v2 < 256) {
                                    _cursor.bg = v2;
                                }
                                else {
                                    ERROR("Colour out of range: " << v2);
                                }
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        default:
                            NYI("Unknown?");
                    }
                }
                break;
            case 49:
                _cursor.bg = Cell::defaultBg();
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
            case 99:
                NYI("Default BRIGHT fg");
                break;
            case 109:
                NYI("Default BRIGHT bg");
                break;

            default:
                // 56..59 Reserved
                // 60..64 (ideogram stuff - hardly ever supported)
                // 90..97 Set foreground colour high intensity - handled separately
                // 100..107 Set background colour high intensity - handled separately

                if (v >= 30 && v < 38) {
                    // normal fg
                    _cursor.fg = v - 30;
                    // BOLD -> += 8
                }
                else if (v >= 40 && v < 48) {
                    // normal bg
                    _cursor.bg = v - 40;
                }
                else if (v >= 90 && v < 98) {
                    // bright fg
                    _cursor.fg = v - 90 + 8;
                }
                else if (v >= 100 && v < 108) {
                    // bright bg
                    _cursor.bg = v - 100 + 8;
                }
                else if (v >= 256 && v < 512) {
                    _cursor.fg = v - 256;
                }
                else if (v >= 512 && v < 768) {
                    _cursor.bg = v - 512;
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
                    _cursor.otherCharSet = false;  // XXX dubious
                    _cursor.G0           = CS_US;
                    _cursor.G1           = CS_US;
                    break;
                case 3: // DECCOLM - Column Mode
                    if (set) {
                        // resize 132x24
                        _observer.terminalResizeBuffer(24, 132);
                    }
                    else {
                        // resize 80x24
                        _observer.terminalResizeBuffer(24, 80);
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
                    _cursor.originMode = set;
                    moveCursorOriginMode(0, 0);
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
                case 47:   // XXX ???
                case 1047:
                    if (_buffer == &_altBuffer) {
                        _buffer->clear();
                    }

                    _buffer = set ? &_altBuffer : &_priBuffer;
                    if (a != 1049) {
                        _buffer->damageAll();
                        break;
                    }
                    // Fall-through:
                case 1048:
                    if (set) {
                        _savedCursor = _cursor;
                    }
                    else {
                        damageCursor();
                        _cursor = _savedCursor;
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
                    _modes.setTo(Mode::ECHO, set);      // XXX correct sense
                    break;
                case 20: // LNM - Linefeed/new line
                    _modes.setTo(Mode::CR_ON_LF, set);
                    break;
                default:
                    ERROR("erresc: unknown set/reset mode: " <<  a);
                    break;
            }
        }
    }
}

std::ostream & operator << (std::ostream & ost, Terminal::Button button) {
    switch (button) {
        case Terminal::Button::LEFT:
            return ost << "left";
        case Terminal::Button::MIDDLE:
            return ost << "middle";
        case Terminal::Button::RIGHT:
            return ost << "right";
    }

    FATAL("Unreachable");
}

std::ostream & operator << (std::ostream & ost, Terminal::ScrollDir dir) {
    switch (dir) {
        case Terminal::ScrollDir::UP:
            return ost << "up";
        case Terminal::ScrollDir::DOWN:
            return ost << "down";
    }

    FATAL("Unreachable");
}
