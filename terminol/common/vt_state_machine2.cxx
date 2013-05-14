// vi:noai:sw=4

#include "terminol/common/vt_state_machine2.hxx"

std::vector<VtStateMachine2::Transition> VtStateMachine2::_globalTransitions;
std::vector<VtStateMachine2::State>      VtStateMachine2::_states;

void VtStateMachine2::initialise() {
    Transition transition;

    transition.matches.push_back(OctetRange::single('a'));
    //_globalTransitions.push_back(
}

void VtStateMachine2::finalise() {
    _globalTransitions.clear();
    _states.clear();
}

VtStateMachine2::VtStateMachine2(I_Observer & observer) :
    _observer(observer)
{
    _observer.machineControl('\0'); // XXX silence compiler
}

VtStateMachine2::~VtStateMachine2() {
}

void VtStateMachine2::consume(utf8::Seq UNUSED(seq), utf8::Length UNUSED(length)) {
    // TODO
}
