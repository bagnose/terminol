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
    _bg(Interlocutor::defaultBg()),
    _fg(Interlocutor::defaultFg()),
    _attributes(),
    _modes(),
    _tabs(_buffer.getCols()),
    _inter(*this, tty)
{
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % Interlocutor::defaultTab() == 0;
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
        _tabs[i] = (i + 1) % Interlocutor::defaultTab() == 0;
    }
}

// Interlocutor::I_Observer implementation:

void Terminal::interBegin() throw () {
    _dispatch = true;
    _observer.terminalBegin();
}

void Terminal::interControl(Control control) throw () {
    PRINT("Control: " << control);
    switch (control) {
        case CONTROL_BEL:
            PRINT("BEL!!");
            break;
        case CONTROL_HT:
            // Advance to the next tab or the last column.
            for (; _cursorCol != _buffer.getCols(); ++_cursorCol) {
                if (_tabs[_cursorCol]) {
                    break;
                }
            }

            if (_cursorCol == _buffer.getCols()) {
                // XXX is this right? CR/LF
                _cursorCol = 0;
                if (_cursorRow == _buffer.getRows() - 1) {
                    _buffer.addLine();
                }
                else {
                    ++_cursorRow;
                }
            }
            break;
        case CONTROL_BS:
            _buffer.overwriteChar(Char::null(), _cursorRow, _cursorCol--);
            break;
        case CONTROL_CR:
            _cursorCol = 0;
            break;
        case CONTROL_LF:
            if (_cursorRow == _buffer.getRows() - 1) {
                _buffer.addLine();
            }
            else {
                ++_cursorRow;
            }
            break;
        default:
            break;
    }

    _observer.terminalDamageAll();
}

void Terminal::interMoveCursor(uint16_t row, uint16_t col) throw () {
    PRINT("Move cursor: " << row << " " << col);
    _observer.terminalDamageChars(_cursorRow, _cursorCol, _cursorCol + 1);
    _cursorRow = clamp<uint16_t>(row, 0, _buffer.getRows() - 1);
    _cursorCol = clamp<uint16_t>(col, 0, _buffer.getCols() - 1);
    _observer.terminalDamageChars(_cursorRow, _cursorCol, _cursorCol + 1);
}

void Terminal::interRelMoveCursor(int16_t dRow, int16_t dCol) throw () {
    PRINT("Rel-move cursor: " << dRow << " " << dCol);
    _observer.terminalDamageChars(_cursorRow, _cursorCol, _cursorCol + 1);
    _cursorRow = clamp<uint16_t>(_cursorRow + dRow, 0, _buffer.getRows() - 1);
    _cursorCol = clamp<uint16_t>(_cursorCol + dCol, 0, _buffer.getCols() - 1);
    _observer.terminalDamageChars(_cursorRow, _cursorCol, _cursorCol + 1);
}

void Terminal::interClearLine(ClearLine clear) throw () {
    PRINT("Clear line: " << clear << " row=" << _cursorRow << ", col=" << _cursorCol);
    switch (clear) {
        case CLEAR_LINE_RIGHT:
            for (uint16_t c = _cursorCol + 1; c != _buffer.getCols(); ++c) {
                _buffer.overwriteChar(Char::null(), _cursorRow, c);
            }
            break;
        case CLEAR_LINE_LEFT:
            for (uint16_t c = 0; c != _cursorCol; ++c) {
                _buffer.overwriteChar(Char::null(), _cursorRow, c);
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
    _buffer.insertChars(_cursorRow, _cursorCol, num);
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

void Terminal::interSetFg(uint8_t fg) throw () {
    _fg = fg;
}

void Terminal::interSetBg(uint8_t bg) throw () {
    _bg = bg;
}

void Terminal::interClearAttributes() throw () {
    //PRINT("Clearing attributes");
    _attributes.clear();
}

void Terminal::interSetAttribute(Attribute attribute, bool value) throw () {
    //PRINT("Setting attribute: " << attribute << " to: " << value);
    _attributes.setTo(attribute, value);
}

void Terminal::interSetMode(Mode mode, bool value) throw () {
    //PRINT("Setting mode: " << mode << " to: " << value);
    _modes.setTo(mode, value);
}

void Terminal::interSetTabStop() throw () {
    PRINT("Setting tab stop at: " << _cursorCol);
    _tabs[_cursorCol] = true;
}

void Terminal::interReset() throw () {
    // TODO consolidate

    _buffer.clearAll();

    _cursorRow = 0;
    _cursorCol = 0;

    _bg = Interlocutor::defaultBg();
    _fg = Interlocutor::defaultFg();

    _modes.clear();
    _attributes.clear();

    _tabs.resize(_buffer.getCols());
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % Interlocutor::defaultTab() == 0;
    }
}

void Terminal::interResetTitle() throw () {
    _observer.terminalResetTitle();
}

void Terminal::interSetTitle(const std::string & title) throw () {
    _observer.terminalSetTitle(title);
}

void Terminal::interUtf8(const char * s, size_t count, size_t UNUSED(size)) throw () {
    for (size_t i = 0; i != count; ++i) {
        utf8::Length length = utf8::leadLength(*s);

        _buffer.overwriteChar(Char::utf8(s, length, _attributes, 0, _fg, _bg),
                              _cursorRow, _cursorCol);

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

        s += length;
    }

    _observer.terminalDamageAll();
}

void Terminal::interEnd() throw () {
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
