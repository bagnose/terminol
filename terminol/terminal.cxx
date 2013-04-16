// vi:noai:sw=4

#include "terminol/terminal.hxx"

Terminal::Terminal(I_Observer & observer,
                   I_Tty      & tty,
                   uint16_t     rows,
                   uint16_t     cols) :
    _observer(observer),
    _dispatch(false),
    _buffer(rows, cols),
    _cursorRow(0),
    _cursorCol(0),
    _bg(Cell::defaultBg()),
    _fg(Cell::defaultFg()),
    _attrs(Cell::defaultAttrs()),
    _modes(),
    _tabs(_buffer.getCols()),
    _inter(*this, tty)
{
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % defaultTab() == 0;
    }
    _modes.set(MODE_WRAP);
}

Terminal::~Terminal() {
    ASSERT(!_dispatch, "");
}

void Terminal::resize(uint16_t rows, uint16_t cols) {
    ASSERT(!_dispatch, "");
    _buffer.resize(rows, cols);
    _tabs.resize(cols);
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % defaultTab() == 0;
    }
}

// Interlocutor::I_Observer implementation:

void Terminal::interBegin() throw () {
    static int seq = 0;
    PRINT("<<<<<<<<< BEGIN: " << seq++);
    _dispatch = true;
    _observer.terminalBegin();
}

void Terminal::interControl(Control control) throw () {
    //PRINT("Control: " << control);
    switch (control) {
        case CONTROL_BEL:
            PRINT("BEL!!");
            break;
        case CONTROL_BS:
            // TODO handle auto-wrap
            if (_cursorCol != 0) {
                --_cursorCol;
            }
            break;
        case CONTROL_HT:
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
        case CONTROL_LF:
            if (_modes.get(MODE_CRLF)) {
                _cursorCol = 0;
            }
            // Fall-through
        case CONTROL_VT:
        case CONTROL_FF:
#if 1
            if (_cursorRow == _buffer.getRows() - 1) {
                _buffer.addLine();
            }
            else {
                ++_cursorRow;
            }
#else
            // FIXME temp hack to make vim work, but stuffs up
            // new line in bash.
            if (_cursorRow != _buffer.getRows() - 1) {
                ++_cursorRow;
            }
#endif

            break;
        case CONTROL_CR:
            _cursorCol = 0;
            break;
        case CONTROL_SO:
            NYI("SO");
            break;
        case CONTROL_SI:
            NYI("SI");
            break;
    }

    _observer.terminalDamageAll();
}

void Terminal::interMoveCursor(uint16_t row, uint16_t col) throw () {
    PRINT("Move cursor: " << row << " " << col);
    _observer.terminalDamageCells(_cursorRow, _cursorCol, _cursorCol + 1);
    _cursorRow = clamp<uint16_t>(row, 0, _buffer.getRows() - 1);
    _cursorCol = clamp<uint16_t>(col, 0, _buffer.getCols() - 1);
    _observer.terminalDamageCells(_cursorRow, _cursorCol, _cursorCol + 1);
}

void Terminal::interRelMoveCursor(int16_t dRow, int16_t dCol) throw () {
    PRINT("Rel-move cursor: " << dRow << " " << dCol);
    _observer.terminalDamageCells(_cursorRow, _cursorCol, _cursorCol + 1);
    _cursorRow = clamp<uint16_t>(_cursorRow + dRow, 0, _buffer.getRows() - 1);
    _cursorCol = clamp<uint16_t>(_cursorCol + dCol, 0, _buffer.getCols() - 1);
    _observer.terminalDamageCells(_cursorRow, _cursorCol, _cursorCol + 1);
}

void Terminal::interClearLine(ClearLine clear) throw () {
    PRINT("Clear line: " << clear << " row=" << _cursorRow << ", col=" << _cursorCol);
    switch (clear) {
        case CLEAR_LINE_RIGHT:
            // XXX is this right?
            for (uint16_t c = _cursorCol; c != _buffer.getCols(); ++c) {
                _buffer.set(_cursorRow, c, Cell::blank());
            }
            break;
        case CLEAR_LINE_LEFT:
            for (uint16_t c = 0; c != _cursorCol; ++c) {
                _buffer.set(_cursorRow, c, Cell::blank());
            }
            break;
        case CLEAR_LINE_ALL:
            _buffer.clearLine(_cursorRow);
            break;
    }
}

void Terminal::interClearScreen(ClearScreen clear) throw () {
    PRINT("Clear screen: " << clear << " row=" << _cursorRow);
    switch (clear) {
        case CLEAR_SCREEN_BELOW:
            for (uint16_t r = _cursorRow + 1; r != _buffer.getRows(); ++r) {
                _buffer.clearLine(r);
            }
            break;
        case CLEAR_SCREEN_ABOVE:
            for (uint16_t r = 0; r != _cursorRow; ++r) {
                _buffer.clearLine(r);
            }
            break;
        case CLEAR_SCREEN_ALL:
            _buffer.clearAll();
            break;
    }

    _observer.terminalDamageAll();
}

void Terminal::interInsertChars(uint16_t num) throw () {
    PRINT("Got insert " << num << " chars with cursor at row: " << _cursorRow << ", col: " << _cursorCol);
    _buffer.insertCells(_cursorRow, _cursorCol, num);
    _observer.terminalDamageAll();
}

void Terminal::interInsertLines(uint16_t num) throw () {
    PRINT("Got insert " << num << " lines with cursor at row: " << _cursorRow);
    _buffer.insertLines(_cursorRow + 1, num);
    _observer.terminalDamageAll();
}

void Terminal::interDeleteLines(uint16_t num) throw () {
    PRINT("Got delete " << num << " lines with cursor at row: " << _cursorRow);
    _buffer.eraseLines(_cursorRow, num);
    _observer.terminalDamageAll();
}

void Terminal::interResetFg() throw () {
    _fg = Cell::defaultFg();
}

void Terminal::interResetBg() throw () {
    _bg = Cell::defaultBg();
}

void Terminal::interSetFg(uint8_t fg) throw () {
    _fg = fg;
}

void Terminal::interSetBg(uint8_t bg) throw () {
    _bg = bg;
}

void Terminal::interClearAttributes() throw () {
    PRINT("Clearing attributes");
    _attrs = Cell::defaultAttrs();
}

void Terminal::interSetAttribute(Attribute attribute, bool value) throw () {
    PRINT("Setting attribute: " << attribute << " to: " << value);
    _attrs.setTo(attribute, value);
}

void Terminal::interSetMode(Mode mode, bool value) throw () {
    PRINT("Setting mode: " << mode << " to: " << value);
    _modes.setTo(mode, value);
}

void Terminal::interSetTabStop() throw () {
    PRINT("Setting tab stop at: " << _cursorCol);
    _tabs[_cursorCol] = true;
}

void Terminal::interAdvanceTab(uint16_t count) throw () {
    NYI("interAdvanceTab: " << count);
}

void Terminal::interSetScrollTopBottom(uint16_t row0, uint16_t row1) {
    FATAL("interSetScrollTopBottom: rows=" << _buffer.getRows() <<
          ", top=" << row0 << ", bottom=" << row1);
}

void Terminal::interSetScrollTop(uint16_t row) {
    FATAL("interSetScrollTop: rows=" << _buffer.getRows() << ", top=" << row);
}

void Terminal::interResetAll() throw () {
    PRINT("Resetting all");
    // TODO consolidate

    _buffer.clearAll();

    _cursorRow = 0;
    _cursorCol = 0;

    _bg    = Cell::defaultBg();
    _fg    = Cell::defaultFg();
    _attrs = Cell::defaultAttrs();

    _modes.clear();

    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % defaultTab() == 0;
    }

    _observer.terminalResetTitle();
}

void Terminal::interSetTitle(const std::string & title) throw () {
    PRINT("Setting title to: " << title);
    _observer.terminalSetTitle(title);
}

void Terminal::interUtf8(const char * s, size_t count, size_t UNUSED(size)) throw () {
    //PRINT("Got UTF-8: " << std::string(s, s + size));
    for (size_t i = 0; i != count; ++i) {
        utf8::Length length = utf8::leadLength(*s);

        _buffer.set(_cursorRow, _cursorCol,
                    Cell::utf8(s, length, _attrs, _fg, _bg));

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

        s += length;
    }

    _observer.terminalDamageAll();
}

void Terminal::interEnd() throw () {
    PRINT(">>>>>>>>> END");
    _observer.terminalEnd();
    _dispatch = false;
}

void Terminal::interGetCursorPos(uint16_t & row, uint16_t & col) const throw () {
    row = _cursorRow;
    col = _cursorCol;
}

void Terminal::interChildExited(int exitStatus) throw () {
    _observer.terminalChildExited(exitStatus);
}
