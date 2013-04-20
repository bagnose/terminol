// vi:noai:sw=4

#include "terminol/common/terminal2.hxx"

#include <algorithm>

Terminal2::Terminal2(I_Observer & observer,
                     I_Tty      & tty) :
    _observer(observer),
    _dispatch(false),
    _tty(tty),
    _dumpWrites(false),
    _writeBuffer(),
    _state(State::NORMAL),
    _escSeq()
{
}

Terminal2::~Terminal2() {
    ASSERT(!_dispatch, "");
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
            }
            break;
        case State::INNER:
            break;
        case State::DCS:
        case State::OSC:
        case State::IGNORE:
            break;
        case State::SPECIAL:
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
            break;
        case BS:
            break;
        case CR:
            break;
        case FF:
            break;
        case VT:
            break;
        case LF:
            break;
        case SO:
            break;
        case SI:
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

void Terminal2::processNormal(utf8::Seq UNUSED(seq), utf8::Length UNUSED(length)) {
}

void Terminal2::processEscape(char UNUSED(c)) {
}
