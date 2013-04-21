// vi:noai:sw=4

#include "terminol/common/terminal2.hxx"

#include <algorithm>

Terminal2::Terminal2(I_Observer & observer,
                     I_Tty      & tty,
                     uint16_t     rows,
                     uint16_t     cols) :
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

Terminal2::~Terminal2() {
    ASSERT(!_dispatch, "");
}

void Terminal2::resize(uint16_t rows, uint16_t cols) {
    ASSERT(!_dispatch, "");
    _buffer.resize(rows, cols);
    _tabs.resize(cols);
    for (size_t i = 0; i != _tabs.size(); ++i) {
        _tabs[i] = (i + 1) % 8 == 0;
    }
}

void Terminal2::read() {
    ASSERT(!_dispatch, "");

    _dispatch = true;
    _observer.terminalBegin();

    try {
        char buffer[4096];
        for (;;) {
            size_t rval = _tty.read(buffer, sizeof buffer);
            if (rval == 0) { break; }

            processRead(buffer, rval);
        }
    }
    catch (I_Tty::Exited & ex) {
        _observer.terminalChildExited(ex.exitCode);
    }

    _observer.terminalEnd();
    _dispatch = false;
}

void Terminal2::write(const char * data, size_t size) {
    ASSERT(!_dispatch, "");

    if (_dumpWrites) { return; }

    if (_writeBuffer.empty()) {
        // Try to write it now, queue what we can't write.
        try {
            while (size != 0) {
                size_t rval = _tty.write(data, size);
                if (rval == 0) { break; }
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

bool Terminal2::areWritesQueued() const {
    ASSERT(!_dispatch, "");
    return !_writeBuffer.empty();
}

void Terminal2::flush() {
    ASSERT(!_dispatch, "");
    ASSERT(areWritesQueued(), "No writes queued.");
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

void Terminal2::processRead(const char * data, size_t size) {
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

void Terminal2::processChar(utf8::Seq seq, utf8::Length len) {
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
                    _state = State::OSC;
                    break;
                case '#':
                case '(':
                case ')':
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
                processCsi();
                _escSeq.clear();
                _state = State::NORMAL;
            }
            break;
        case State::INNER:
            if (lead == '\\') {
                if (_outerState == State::DCS) {
                    processDcs();
                }
                else if (_outerState == State::OSC) {
                    processOsc();
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
                processOsc();
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
                processSpecial();
            }
            _escSeq.clear();
            _state = State::NORMAL;
            break;
        default:
            break;
    }
}

void Terminal2::processControl(char c) {
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
            // Fall-through
        case FF:
        case VT:
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

void Terminal2::processNormal(utf8::Seq seq, utf8::Length length) {
    _buffer.set(_cursorRow, _cursorCol, Cell::utf8(seq.bytes, length, _attrs, _fg, _bg));

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

void Terminal2::processEscape(char UNUSED(c)) {
}

void Terminal2::processCsi() {
}

void Terminal2::processDcs() {
}

void Terminal2::processOsc() {
}

void Terminal2::processSpecial() {
}
