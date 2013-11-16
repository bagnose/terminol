// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/vt_state_machine.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/support/escape.hxx"

namespace {

    // [0x30..0x7F)
    const char * SIMPLE_STR[] = {
        nullptr,    // '0'              0x30
        nullptr,    // '1'
        nullptr,    // '2'
        nullptr,    // '3'
        nullptr,    // '4'
        nullptr,    // '5'
        nullptr,    // '6'
        "DECSC",    // '7'
        //
        "DECRC",    // '8'
        nullptr,    // '9'
        nullptr,    // ':'
        nullptr,    // ';'
        nullptr,    // '<'
        "DECKPAM",  // '='
        "DECKPNM",  // '>'
        nullptr,    // '?'
        //
        nullptr,    // '@'              0x40
        nullptr,    // 'A'
        nullptr,    // 'B'
        nullptr,    // 'C'
        "IND",      // 'D'
        "NEL",      // 'E'
        nullptr,    // 'F'
        nullptr,    // 'G'
        //
        "HTS",      // 'H'
        nullptr,    // 'I'
        nullptr,    // 'J'
        nullptr,    // 'K'
        nullptr,    // 'L'
        "RI",       // 'M'
        "SS2",      // 'N'
        "SS3",      // 'O'
        //
        nullptr,    // 'P'              0x50
        nullptr,    // 'Q'
        nullptr,    // 'R'
        nullptr,    // 'S'
        nullptr,    // 'T'
        nullptr,    // 'U'
        nullptr,    // 'V'
        nullptr,    // 'W'
        //
        nullptr,    // 'X'
        nullptr,    // 'Y'
        "DECID",    // 'Z'
        nullptr,    // '['
        nullptr,    // '\'
        nullptr,    // ']'
        nullptr,    // '^'
        nullptr,    // '_'
        //
        nullptr,    // '`'              0x60
        nullptr,    // 'a'
        nullptr,    // 'b'
        "RIS",      // 'c'
        nullptr,    // 'd'
        nullptr,    // 'e'
        nullptr,    // 'f'
        nullptr,    // 'g'
        //
        nullptr,    // 'h'
        nullptr,    // 'i'
        nullptr,    // 'j'
        nullptr,    // 'k'
        nullptr,    // 'l'
        nullptr,    // 'm'
        nullptr,    // 'n'
        nullptr,    // 'o'
        //
        nullptr,    // 'p'              0x70
        nullptr,    // 'q'
        nullptr,    // 'r'
        nullptr,    // 's'
        nullptr,    // 't'
        nullptr,    // 'u'
        nullptr,    // 'v'
        nullptr,    // 'w'
        //
        nullptr,    // 'x'
        nullptr,    // 'y'
        nullptr,    // 'z'
        nullptr,    // '{'
        nullptr,    // '|'
        nullptr,    // '}'
        nullptr     // '~'
    };

    static_assert(STATIC_ARRAY_SIZE(SIMPLE_STR) == 0x7F - 0x30,
                  "Incorrect size: SIMPLE_STR.");

    // [0x40..0x80)
    const char * CSI_STR[] = {
        "ICH",      // '@'              0x40
        "CUU",      // 'A'
        "CUD",      // 'B'
        "CUF",      // 'C'
        "CUB",      // 'D'
        "CNL",      // 'E'
        "CPL",      // 'F'
        "CHA",      // 'G'
        //
        "CUP",      // 'H'
        "CHT",      // 'I'
        "ED",       // 'J'
        "EL",       // 'K'
        "IL",       // 'L'
        "DL",       // 'M'
        nullptr,    // 'N'      EF
        nullptr,    // 'O'      EA
        //
        "DCH",      // 'P'              0x50
        nullptr,    // 'Q'      SEE
        nullptr,    // 'R'      CPR
        "SU",       // 'S'
        "SD",       // 'T'
        nullptr,    // 'U'      NP
        nullptr,    // 'V'      PP
        "CTC",      // 'W'
        //
        "ECH",      // 'X'
        nullptr,    // 'Y'      CVT
        "CBT",      // 'Z'
        nullptr,    // '['      SRS
        nullptr,    // '\'      PTX
        nullptr,    // ']'      SDS
        nullptr,    // '^'      SIMD
        nullptr,    // '_'      5F
        //
        "HPA",      // '`'              0x60
        "HPR",      // 'a'
        "REP",      // 'b'
        "DA",       // 'c'
        "VPA",      // 'd'
        "VPR",      // 'e'
        "HVP",      // 'f'
        "TBC",      // 'g'
        //
        "SM",       // 'h'
        nullptr,    // 'i'      MC
        nullptr,    // 'j'      HPB
        nullptr,    // 'k'      VPB
        "RM",       // 'l'
        "SGR",      // 'm'
        "DSR",      // 'n'
        nullptr,    // 'o'      DAQ
        //
        nullptr,    // 'p'              0x70
        "SCA",      // 'q'
        "STBM",     // 'r'
        nullptr,    // 's'      save cursor?
        nullptr,    // 't'      window ops?
        nullptr,    // 'u'      restore cursor
        nullptr,    // 'v'
        nullptr,    // 'w'
        //
        nullptr,    // 'x'
        nullptr,    // 'y'
        nullptr,    // 'z'
        nullptr,    // '{'
        nullptr,    // '|'
        nullptr,    // '}'
        nullptr,    // '~'
        nullptr     // DEL
    };

    static_assert(STATIC_ARRAY_SIZE(CSI_STR) == 0x80 - 0x40,
                  "Incorrect size: CSI_STR.");

} // namespace {anonymous}

std::ostream & operator << (std::ostream & ost, const SimpleEsc & esc) {
    ASSERT(esc.code >= 0x30 && esc.code < 0x7F,
           "Simple escape code out of range: " <<
           std::hex << static_cast<int>(esc.code));

    // escape initiator
    ost << "^[";

    // intermediates
    for (auto i : esc.inters) {
        ost << i;
    }

    // code
    auto str = SIMPLE_STR[esc.code - 0x30];
    if (str) {
        ost << '(' << str << ')';
    }
    else {
        ost << esc.code;
    }

    return ost;
}

std::ostream & operator << (std::ostream & ost, const CsiEsc & esc) {
    ASSERT(esc.mode >= 0x40 && esc.mode < 0x80,
           "CSI Escape mode out of range: " <<
           std::hex << static_cast<int>(esc.mode));

    // CSI initiator
    ost << "^[[";

    // private
    if (esc.priv != NUL) { ost << esc.priv; }

    // arguments
    auto firstArg = true;
    for (auto a : esc.args) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << a;
    }

    // intermediates
    for (auto i : esc.inters) {
        ost << i;
    }

    // mode
    auto str = CSI_STR[esc.mode - 0x40];
    if (str) {
        ost << '(' << str << ')';
    }
    else {
        ost << esc.mode;
    }

    return ost;
}

std::ostream & operator << (std::ostream & ost, const DcsEsc & esc) {
    // DCS initiator
    ost << "^[P";

    for (auto s : esc.seq) {
        ost << s;
    }

    return ost;
}

std::ostream & operator << (std::ostream & ost, const OscEsc & esc) {
    // OSC initiator
    ost << "^[]";

    // arguments
    auto firstArg = true;
    for (auto a : esc.args) {
        if (firstArg) { firstArg = false; }
        else          { ost << ';'; }
        ost << a;
    }

    return ost;
}

namespace {

bool inRange(uint8_t c, uint8_t min, uint8_t max) {
    return c >= min && c <= max;
}

} // namespace {anonymous}

VtStateMachine::VtStateMachine(I_Observer   & observer,
                               const Config & config) :
    _observer(observer),
    _config(config),
    _state(State::GROUND),
    _escSeq() {}

void VtStateMachine::consume(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (c == 0x18 /* CAN */ || c == 0x1A /* SUB */) {
            _state = State::GROUND;
            return;
        }
        else if (c == 0x1B /* ESC */) {
            if (_state == State::OSC_STRING) {
                processOsc(_escSeq);
            }

            _state = State::ESCAPE;
            _escSeq.clear();
            return;
        }
    }

    switch (_state) {
        case GROUND:
            ground(seq, length);
            break;
        case ESCAPE_INTERMEDIATE:
            escapeIntermediate(seq, length);
            break;
        case ESCAPE:
            escape(seq, length);
            break;
        case SOS_PM_APC_STRING:
            sosPmApcString(seq, length);
            break;
        case CSI_ENTRY:
            csiEntry(seq, length);
            break;
        case CSI_PARAM:
            csiParam(seq, length);
            break;
        case CSI_IGNORE:
            csiIgnore(seq, length);
            break;
        case CSI_INTERMEDIATE:
            csiIntermediate(seq, length);
            break;
        case OSC_STRING:
            oscString(seq, length);
            break;
        case DCS_ENTRY:
            dcsEntry(seq, length);
            break;
        case DCS_PARAM:
            dcsParam(seq, length);
            break;
        case DCS_IGNORE:
            dcsIgnore(seq, length);
            break;
        case DCS_INTERMEDIATE:
            dcsIntermediate(seq, length);
            break;
        case DCS_PASSTHROUGH:
            dcsPassthrough(seq, length);
            break;
    }
}

void VtStateMachine::ground(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x7F /* DEL */)) {
            if (_config.traceTty) {
                std::cerr << SGR::FG_GREEN << SGR::UNDERLINE << seq << SGR::RESET_ALL;
            }
            _observer.machineNormal(seq, length);
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        _observer.machineNormal(seq, length);
    }
}

void VtStateMachine::escape(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x30 /* 0 */, 0x4F /* O */) ||
                 inRange(c, 0x51 /* Q */, 0x57 /* W */) ||
                 c == 0x59 /* Y */ || c == 0x5A /* Z */ || c == 0x5C /* \ */ ||
                 inRange(c, 0x60 /* ` */, 0x7E /* ~ */)) {
            _escSeq.push_back(c);
            processEsc(_escSeq);
            _state = State::GROUND;
        }
        else if (c == 0x58 || c == 0x5E || c == 0x5F) {
            _state = State::SOS_PM_APC_STRING;
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            // collect
            _escSeq.push_back(c);
            _state = State::ESCAPE_INTERMEDIATE;
        }
        else if (c == 0x5B /* [ */) {
            _state = State::CSI_ENTRY;
        }
        else if (c == 0x5D /* ] */) {
            _state = State::OSC_STRING;
        }
        else if (c == 0x50 /* P */) {
            _state = State::DCS_ENTRY;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

void VtStateMachine::escapeIntermediate(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x30 /* 0 */, 0x7E /* ~ */)) {
            _escSeq.push_back(c);
            processEsc(_escSeq);
            _state = State::GROUND;
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            // collect
            _escSeq.push_back(c);
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

void VtStateMachine::sosPmApcString(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x7F /* DEL */)) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");          // XXX is UTF-8 allowed here?
        _state = State::GROUND;
    }
}

void VtStateMachine::csiEntry(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            _escSeq.push_back(c);
            _state = State::CSI_INTERMEDIATE;
        }
        else if (c == 0x3A /* : */) {
            _state = State::CSI_IGNORE;
        }
        else if (inRange(c, 0x30 /* 0 */, 0x39 /* 9 */) || c == 0x3B /* ; */) {
            // param
            _escSeq.push_back(c);
            _state = State::CSI_PARAM;
        }
        else if (inRange(c, 0x3C /* < */, 0x3F /* ? */)) {
            // collect
            _escSeq.push_back(c);
            _state = State::CSI_PARAM;
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            // dispatch
            _escSeq.push_back(c);
            processCsi(_escSeq);
            _state = State::GROUND;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }

}

void VtStateMachine::csiParam(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            _escSeq.push_back(c);
            _state = State::CSI_INTERMEDIATE;
        }
        else if (inRange(c, 0x30 /* 0 */, 0x39 /* 9 */) || c == 0x3B /* ; */) {
            // param
            _escSeq.push_back(c);
        }
        else if (c == 0x3A || inRange(c, 0x3C /* < */, 0x3F /* ? */)) {
            _state = State::CSI_IGNORE;
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            // dispatch
            _escSeq.push_back(c);
            processCsi(_escSeq);
            _state = State::GROUND;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

void VtStateMachine::csiIgnore(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x3F /* ? */)) {
            // ignore
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            // no dispatch
            _state = State::GROUND;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

void VtStateMachine::csiIntermediate(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            processControl(c);
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            // collect
            _escSeq.push_back(c);
        }
        else if (inRange(c, 0x30 /* 0 */, 0x3F /* ? */)) {
            _state = State::CSI_IGNORE;
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            // dispatch
            _escSeq.push_back(c);
            processCsi(_escSeq);
            _state = State::GROUND;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

void VtStateMachine::oscString(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (c == 0x07 /* BEL */) {        // XXX parser specific
            // ST (ESC \)
            _state = State::GROUND;
            processOsc(_escSeq);
            return;
        }

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x7F /* DEL */)) {
            // put
            _escSeq.push_back(c);
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        // put
        std::copy(seq.bytes, seq.bytes + size_t(length), std::back_inserter(_escSeq));
    }
}

void VtStateMachine::dcsEntry(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            // collect
            _escSeq.push_back(c);
            _state = State::DCS_INTERMEDIATE;
        }
        else if (c == 0x3A) {
            _state = State::DCS_IGNORE;
        }
        else if (inRange(c, 0x30 /* 0 */, 0x39 /* 9 */) || c == 0x3B /* ; */) {
            // param
            _escSeq.push_back(c);
            _state = State::DCS_PARAM;
        }
        else if (inRange(c, 0x3C /* < */, 0x3F /* ? */)) {
            // collect
            _escSeq.push_back(c);
            _state = State::DCS_PARAM;
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            _state = State::DCS_PASSTHROUGH;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

void VtStateMachine::dcsParam(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x7F /* DEL */)) {
            // collect
            _escSeq.push_back(c);
            _state = State::DCS_INTERMEDIATE;
        }
        else if (inRange(c, 0x30 /* 0 */, 0x39 /* 9 */) || c == 0x3B /* ; */) {
            // param
            _escSeq.push_back(c);
        }
        else if (c == 0x3A || inRange(c, 0x3C /* < */, 0x3F /* ? */)) {
            _state = State::DCS_IGNORE;
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            _state = State::DCS_PASSTHROUGH;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");      // XXX is UTF-8 allowed here?
        _state = State::GROUND;
    }
}

void VtStateMachine::dcsIgnore(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x7F /* DEL */)) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");      // XXX is UTF-8 allowed here?
        _state = State::GROUND;
    }
}

void VtStateMachine::dcsIntermediate(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20 /* SPACE */, 0x2F /* / */)) {
            // collect
            _escSeq.push_back(c);
        }
        else if (inRange(c, 0x30 /* 0 */, 0x3F /* ? */)) {
            _state = State::DCS_IGNORE;
        }
        else if (inRange(c, 0x40 /* @ */, 0x7E /* ~ */)) {
            _state = State::DCS_PASSTHROUGH;
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");      // XXX is UTF-8 allowed here?
        _state = State::GROUND;
    }
}

void VtStateMachine::dcsPassthrough(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        auto c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F) ||
            inRange(c, 0x20 /* SPACE */, 0x7E /* ~ */)) {
            // TODO put
        }
        else if (c == 0x7F /* DEL */) {
            // ignore
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        ERROR("Unexpected UTF-8");      // XXX is UTF-8 allowed here?
        _state = State::GROUND;
    }
}

//
//
//

void VtStateMachine::processControl(uint8_t c) {
    if (_config.traceTty) {
        std::cerr << SGR::FG_YELLOW << Char(c) << SGR::RESET_ALL;
        if (c == LF || c == FF || c == VT) {
            std::cerr << std::endl;
        }
    }
    _observer.machineControl(c);
}

void VtStateMachine::processEsc(const std::vector<uint8_t> & seq) {
    ASSERT(!seq.empty(), "");

    SimpleEsc esc;
    esc.inters.insert(esc.inters.begin(), seq.begin(), seq.end() - 1);
    esc.code = seq.back();

    if (_config.traceTty) {
        std::cerr << SGR::FG_CYAN << esc << SGR::RESET_ALL;
    }
    _observer.machineSimpleEsc(esc);
}

void VtStateMachine::processCsi(const std::vector<uint8_t> & seq) {
    ASSERT(seq.size() >= 1, "");

    size_t i = 0;
    CsiEsc esc;

    // Private:

    if (inRange(seq[i], 0x3C /* < */, 0x3F /* ? */)) {
        esc.priv = seq[i];
        ++i;
    }
    else {
        esc.priv = NUL;
    }

    // Arguments:

    auto inArg = false;

    while (i != seq.size()) {
        auto c = seq[i];

        if (c >= '0' && c <= '9') {
            if (!inArg) { esc.args.push_back(0); inArg = true; }
            esc.args.back() = 10 * esc.args.back() + c - '0';
        }
        else {
            if (c != ';') { break; }
            if (inArg) { inArg = false; }
        }

        ++i;
    }

    // Intermediates:

    while (inRange(seq[i], 0x20 /* SPACE */, 0x2F /* ? */)) {
        esc.inters.push_back(seq[i]);
        ++i;
    }

    ASSERT(i == seq.size() - 1, "");

    // Mode:

    esc.mode = seq[i];

    // Dispatch:

    if (_config.traceTty) {
        std::cerr << SGR::FG_GREEN << esc << SGR::RESET_ALL;
    }
    _observer.machineCsiEsc(esc);
}

void VtStateMachine::processOsc(const std::vector<uint8_t> & seq) {
    OscEsc esc;

    auto next = true;
    for (auto c : seq) {
        if (next) { esc.args.push_back(std::string()); next = false; }

        if (c == ';') { next = true; }
        else          { esc.args.back().push_back(c); }
    }

    // Dispatch:

    if (_config.traceTty) {
        std::cerr << SGR::FG_RED << esc << SGR::RESET_ALL;
    }
    _observer.machineOscEsc(esc);
}
