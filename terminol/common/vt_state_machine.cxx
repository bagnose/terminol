// vi:noai:sw=4

#include "terminol/common/vt_state_machine.hxx"
#include "terminol/common/ascii.hxx"

namespace {

bool inRange(uint8_t c, uint8_t min, uint8_t max) {
    return c >= min && c <= max;
}

} // namespace {anonymous}

VtStateMachine::VtStateMachine(I_Observer & observer) :
    _observer(observer),
    _state(State::GROUND),
    _escSeq() {}

void VtStateMachine::consume(utf8::Seq seq, utf8::Length length) {
    /*
    if (length == utf8::Length::L1) {
        std::cerr << int(seq.lead()) << seq.lead() << std::endl;
    }
    else {
        std::cerr << seq << std::endl;
    }
    */

    if (length == utf8::Length::L1) {
        uint8_t c = seq.lead();

        if (c == 0x18 /* CAN */ || c == 0x1A /* SUB */) {
            _state = State::GROUND;
            return;
        }
        else if (c == 0x1B /* ESC */) {
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
            csiIngore(seq, length);
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x20, 0x7F)) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x30, 0x4F) || inRange(c, 0x51, 0x57) ||
                 c == 0x59 || c == 0x5A || c == 0x5C || inRange(c, 0x60, 0x7E)) {
            _escSeq.push_back(c);
            processEsc(_escSeq);
            _state = State::GROUND;
        }
        else if (c == 0x58 || c == 0x5E || c == 0x5F) {
            _state = State::SOS_PM_APC_STRING;
        }
        else if (inRange(c, 0x20, 0x2F)) {
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
        else if (c == 0x7F) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x30, 0x7E)) {
            _escSeq.push_back(c);
            processEsc(_escSeq);
            _state = State::GROUND;
        }
        else if (inRange(c, 0x20, 0x2F)) {
            // collect
            _escSeq.push_back(c);
        }
        else if (c == 0x7F) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20, 0x7F)) {
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

void VtStateMachine::csiEntry(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x20, 0x2F)) {
            _state = State::CSI_INTERMEDIATE;
        }
        else if (c == 0x3A /* : */) {
            _state = State::CSI_IGNORE;
        }
        else if (inRange(c, 0x30, 0x39) || c == 0x3B) {
            // param
            _escSeq.push_back(c);
            _state = State::CSI_PARAM;
        }
        else if (inRange(c, 0x3C, 0x3F)) {
            // collect
            _escSeq.push_back(c);
            _state = State::CSI_PARAM;
        }
        else if (inRange(c, 0x40, 0x7E)) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x30, 0x39) || c == 0x3B) {
            // param
            _escSeq.push_back(c);
        }
        else if (c == 0x3A || inRange(c, 0x3C, 0x3F)) {
            _state = State::CSI_IGNORE;
        }
        else if (inRange(c, 0x40, 0x7E)) {
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

void VtStateMachine::csiIngore(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x20, 0x3F)) {
            // ignore
        }
        else if (inRange(c, 0x40, 0x7E)) {
            // no dispatch
            _state = State::GROUND;
        }
        else if (c == 0x7F) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            _observer.machineControl(c);
        }
        else if (inRange(c, 0x20, 0x2F)) {
            // collect
            _escSeq.push_back(c);
        }
        else if (inRange(c, 0x30, 0x3F)) {
            _state = State::CSI_IGNORE;
        }
        else if (inRange(c, 0x40, 0x7E)) {
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
        uint8_t c = seq.lead();

        if (c == 0x07 /* BEL */) {        // XXX parser specific
            // ST (ESC \)
            _state = State::GROUND;
            processOsc(_escSeq);
            return;
        }

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20, 0x7F)) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20, 0x2F)) {
            // collect
            _escSeq.push_back(c);
            _state = State::DCS_INTERMEDIATE;
        }
        else if (c == 0x3A) {
            _state = State::DCS_IGNORE;
        }
        else if (inRange(c, 0x30, 0x39) || c == 0x3B) {
            // param
            _escSeq.push_back(c);
            _state = State::DCS_PARAM;
        }
        else if (inRange(c, 0x3C, 0x3F)) {
            // collect
            _escSeq.push_back(c);
            _state = State::DCS_PARAM;
        }
        else if (inRange(c, 0x40, 0x7E)) {
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
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20, 0x7F)) {
            // collect
            _escSeq.push_back(c);
            _state = State::DCS_INTERMEDIATE;
        }
        else if (inRange(c, 0x30, 0x39) || c == 0x3B) {
            // param
            _escSeq.push_back(c);
        }
        else if (c == 0x3A || inRange(c, 0x3C, 0x3F)) {
            _state = State::DCS_IGNORE;
        }
        else if (inRange(c, 0x40, 0x7E)) {
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

void VtStateMachine::dcsIgnore(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20, 0x7F)) {
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

void VtStateMachine::dcsIntermediate(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // ignore
        }
        else if (inRange(c, 0x20, 0x2F)) {
            // collect
            _escSeq.push_back(c);
        }
        else if (inRange(c, 0x30, 0x3F)) {
            _state = State::DCS_IGNORE;
        }
        else if (inRange(c, 0x40, 0x7E)) {
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

void VtStateMachine::dcsPassthrough(utf8::Seq seq, utf8::Length length) {
    if (length == utf8::Length::L1) {
        uint8_t c = seq.lead();

        if (inRange(c, 0x00, 0x17) || c == 0x19 || inRange(c, 0x1C, 0x1F)) {
            // TODO put
        }
        else if (inRange(c, 0x20, 0x7E)) {
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
        ERROR("Unexpected UTF-8");
        _state = State::GROUND;
    }
}

//
//
//

void VtStateMachine::processEsc(const std::vector<uint8_t> & seq) {
    //PRINT("ESC: " << Str(seq));

    if (seq.size() == 1) {
        _observer.machineEscape(seq.back());
    }
    else {
        _observer.machineSpecial(seq.front(), seq.back());
    }
}

void VtStateMachine::processCsi(const std::vector<uint8_t> & seq) {
    //PRINT("CSI: " << Str(seq));

    ASSERT(seq.size() >= 1, "");

    size_t i = 0;
    uint8_t priv = 0;
    std::vector<int32_t> args;

    //
    // Parse the arguments.
    //

    if (inRange(seq[i], 0x3C /* < */, 0x3F /* ? */)) {
        priv = seq[i];
        ++i;
    }

    bool inArg = false;

    while (i != seq.size()) {
        uint8_t c = seq[i];

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

    /*
    for (;;) {
        if (seq[i] == '>') {
        }
        else {
            break;
        }

        ++i;
    }
    */

    ASSERT(i == seq.size() - 1, "i=" << i << ", seq.size=" << seq.size() << ", Seq: " << Str(seq));

    _observer.machineCsi(priv, args, seq[i]);
}

void VtStateMachine::processOsc(const std::vector<uint8_t> & seq) {
    PRINT("OSC: " << Str(seq));

    ASSERT(!seq.empty(), "");

    size_t i = 0;
    std::vector<std::string> args;

    //
    // Parse the arguments.
    //

    bool next = true;
    while (i != seq.size()) {
        uint8_t c = seq[i];

        if (next) { args.push_back(std::string()); next = false; }

        if (c == ';') { next = true; }
        else          { args.back().push_back(c); }

        ++i;
    }

    _observer.machineOsc(args);
}

void VtStateMachine::processSpecial(const std::vector<uint8_t> & seq) {
    //PRINT("SPC: " << Str(seq));

    ASSERT(seq.size() == 2, "");
    uint8_t special = seq.front();
    uint8_t code    = seq.back();

    _observer.machineSpecial(special, code);
}
