// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__VT_STATE_MACHINE__H
#define COMMON__VT_STATE_MACHINE__H

#include "terminol/common/utf8.hxx"
#include "terminol/common/config.hxx"

#include <vector>

#include <stdint.h>

//
// Simple Escape.
//

struct SimpleEsc {
    std::vector<uint8_t> inters;
    uint8_t              code;
};

std::ostream & operator << (std::ostream & ost, const SimpleEsc & esc);

//
// Control Sequence Initiator.
//

struct CsiEsc {
    CsiEsc() : priv('\0'), args(), inters(), mode('\0') {}

    uint8_t              priv;
    std::vector<int32_t> args;
    std::vector<uint8_t> inters;
    uint8_t              mode;
};

std::ostream & operator << (std::ostream & ost, const CsiEsc & esc);

//
// Device Control Sequence.
//

struct DcsEsc {
    std::vector<uint8_t> seq;
};

std::ostream & operator << (std::ostream & ost, const DcsEsc & esc);

//
// Operating System Command.
//

struct OscEsc {
    std::vector<std::string> args;
};

std::ostream & operator << (std::ostream & ost, const OscEsc & esc);

//
//
//

class VtStateMachine {
public:
    class I_Observer {
    public:
        virtual void machineNormal(utf8::Seq seq, utf8::Length length) throw () = 0;
        virtual void machineControl(uint8_t control) throw () = 0;
        virtual void machineSimpleEsc(const SimpleEsc & esc) throw () = 0;
        virtual void machineCsiEsc(const CsiEsc & esc) throw () = 0;
        virtual void machineDcsEsc(const DcsEsc & esc) throw () = 0;
        virtual void machineOscEsc(const OscEsc & esc) throw () = 0;

    protected:
        ~I_Observer() {}
    };

private:
    enum State : uint8_t {
        GROUND,
        ESCAPE,
        ESCAPE_INTERMEDIATE,
        SOS_PM_APC_STRING,
        CSI_ENTRY,
        CSI_PARAM,
        CSI_IGNORE,
        CSI_INTERMEDIATE,
        OSC_STRING,
        DCS_ENTRY,
        DCS_PARAM,
        DCS_IGNORE,
        DCS_INTERMEDIATE,
        DCS_PASSTHROUGH
    };

    I_Observer           & _observer;
    const Config         & _config;
    State                  _state;
    std::vector<uint8_t>   _escSeq;

public:
    VtStateMachine(I_Observer & observer, const Config & config);

    void consume(utf8::Seq seq, utf8::Length length);

protected:
    void ground(utf8::Seq seq, utf8::Length length);
    void escapeIntermediate(utf8::Seq seq, utf8::Length length);
    void escape(utf8::Seq seq, utf8::Length length);
    void sosPmApcString(utf8::Seq seq, utf8::Length length);
    void csiEntry(utf8::Seq seq, utf8::Length length);
    void csiParam(utf8::Seq seq, utf8::Length length);
    void csiIgnore(utf8::Seq seq, utf8::Length length);
    void csiIntermediate(utf8::Seq seq, utf8::Length length);
    void oscString(utf8::Seq seq, utf8::Length length);
    void dcsEntry(utf8::Seq seq, utf8::Length length);
    void dcsParam(utf8::Seq seq, utf8::Length length);
    void dcsIgnore(utf8::Seq seq, utf8::Length length);
    void dcsIntermediate(utf8::Seq seq, utf8::Length length);
    void dcsPassthrough(utf8::Seq seq, utf8::Length length);


    void processControl(uint8_t c);
    void processEsc(const std::vector<uint8_t> & seq);
    void processCsi(const std::vector<uint8_t> & seq);
    void processOsc(const std::vector<uint8_t> & seq);
};

#endif // COMMON__VT_STATE_MACHINE__H
