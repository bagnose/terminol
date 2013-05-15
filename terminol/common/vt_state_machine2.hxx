// vi:noai:sw=4

#ifndef COMMON__VT_STATE_MACHINE2__H
#define COMMON__VT_STATE_MACHINE2__H

#include "terminol/common/utf8.hxx"
//#include "terminol/support/support.hxx"

#include <map>
#include <vector>

#include <stdint.h>

class VtStateMachine2 {
public:
    static void initialise();
    static void finalise();

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
    I_Observer & _observer;

    enum Name : size_t {
        GROUND,
        ESCAPE_INTERMEDIATE,
        ESCAPE,
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

    typedef uint8_t Octet;      // Move into utf8, or ascii?

    struct OctetRange {
        static OctetRange single(Octet octet) {
            return OctetRange(octet, octet + 1);
        }

        static OctetRange multi(Octet begin, Octet end) {
            return OctetRange(begin, end);
        }

        Octet begin;
        Octet end;

        bool contains(Octet octet) {
            return begin <= octet && (end == '\0' || octet < end);
        }

    private:
        OctetRange(Octet begin_, Octet end_) : begin(begin_), end(end_) {}
    };

    typedef void (VtStateMachine2::*Method)();

    struct Transition {
        std::vector<OctetRange> matches;
        Method                  action;
        Name                    destination;
    };

    struct State {
        Method                  enterAction;
        std::vector<Transition> transitions;
        Method                  exitAction;
    };

    static std::vector<Transition> _globalTransitions;
    static std::vector<State>      _states;

public:
    VtStateMachine2(I_Observer & observer);
    ~VtStateMachine2();

    void consume(utf8::Seq seq, utf8::Length length);
};

#endif // COMMON__VT_STATE_MACHINE__H
