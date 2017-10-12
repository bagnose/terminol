// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/vt_state_machine.hxx"
#include "terminol/common/ascii.hxx"
#include "terminol/common/escape.hxx"

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
                std::cerr
                    << CsiEsc::SGR(CsiEsc::StockSGR::FG_GREEN)
                    << CsiEsc::SGR(CsiEsc::StockSGR::UNDERLINE)
                    << seq
                    << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL);
            }
            _observer.machineNormal(seq, length);
        }
        else {
            ERROR("Unexpected: " << seq);
        }
    }
    else {
        if (_config.traceTty) {
            std::cerr
                << CsiEsc::SGR(CsiEsc::StockSGR::FG_GREEN)
                << CsiEsc::SGR(CsiEsc::StockSGR::UNDERLINE)
                << seq
                << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL);
        }
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
        std::copy(seq.bytes.begin(), seq.bytes.begin() + length, std::back_inserter(_escSeq));
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
        std::cerr
            << CsiEsc::SGR(CsiEsc::StockSGR::FG_YELLOW)
            << Char{c}
            << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL);
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
        std::cerr
            << CsiEsc::SGR(CsiEsc::StockSGR::FG_CYAN)
            << esc.str()
            << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL);
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
        std::cerr
            << CsiEsc::SGR(CsiEsc::StockSGR::FG_WHITE)
            << esc.str()
            << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL);
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
        std::cerr
            << CsiEsc::SGR(CsiEsc::StockSGR::FG_RED)
            << esc.str()
            << CsiEsc::SGR(CsiEsc::StockSGR::RESET_ALL);
    }
    _observer.machineOscEsc(esc);
}
