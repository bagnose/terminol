// vi:noai:sw=4

#ifndef COMMON__VT_STATE_MACHINE2__H
#define COMMON__VT_STATE_MACHINE2__H

#include "terminol/common/utf8.hxx"
#if 0
#include <map>
#include <vector>

#include <stdint.h>

typedef uint8_t Octet;

class StateMachine {
public:
    static void initialise();
    static void finalise();

    class I_Observer {
    public:
        virtual void machineNormal(utf8::Seq seq) throw () = 0;
        virtual void machineCsi(const std::vector<int32_t> & args,
                                Octet code) throw () = 0;
        //virtual void machineOsc(

    protected:
        I_Observer() {}
        ~I_Observer() {}
    };

private:
    enum Name : size_t {
        GROUND,
        LAST_NAME
    };

    struct OctetRange {
        Octet begin;
        Octet end;

        bool contains(Octet octet) {
            return begin <= octet && (end == NUL || octet < end);
        }
    };

    typedef void (StateMachine::*Method)();

    struct State {
        Method      enter;
        Transitions transitions;
        Method      exit;
    };

    static const State      STATES[LAST_NAME];
    static const Transition TRANSITIONS[];

    Transitions _globalTransitions;

public:
    StateMachine(I_Observer & observer);

    void process(utf8::Seq seq);
};
#endif

#endif // COMMON__VT_STATE_MACHINE__H
