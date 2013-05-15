// vi:noai:sw=4

#ifndef COMMON__VT_STATE_MACHINE__H
#define COMMON__VT_STATE_MACHINE__H

#include "terminol/common/utf8.hxx"
//#include "terminol/support/support.hxx"

#include <vector>

#include <stdint.h>

class VtStateMachine {
public:
    class I_Observer {
    public:
        virtual void machineNormal(utf8::Seq seq, utf8::Length length) throw () = 0;
        virtual void machineControl(uint8_t c) throw () = 0;
        virtual void machineEscape(uint8_t c) throw () = 0;
        virtual void machineCsi(bool priv,
                                const std::vector<int32_t> & args,
                                uint8_t mode) throw () = 0;
        virtual void machineDcs(const std::vector<uint8_t> & seq) throw () = 0;
        virtual void machineOsc(const std::vector<std::string> & args) throw () = 0;
        virtual void machineSpecial(uint8_t special, uint8_t code) throw () = 0;

    protected:
        I_Observer() {}
        ~I_Observer() {}
    };

private:
    enum class State {
        NORMAL,
        ESCAPE,
        DCS,
        CSI,
        OSC,
        INNER,
        IGNORE,
        SPECIAL
    };

    I_Observer           & _observer;

    State                  _state;
    State                  _outerState;
    std::vector<uint8_t>   _escSeq;

public:
    VtStateMachine(I_Observer & observer);

    void consume(utf8::Seq seq, utf8::Length length);

protected:
    void processCsi(const std::vector<uint8_t> & seq);
    void processOsc(const std::vector<uint8_t> & seq);
    void processSpecial(const std::vector<uint8_t> & seq);
};

#endif // COMMON__VT_STATE_MACHINE__H
