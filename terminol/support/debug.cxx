// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"

#include <cstdlib>

namespace {

    void defaultTerminate() { std::terminate(); }

    TerminateHandler terminateHandler = &defaultTerminate;

} // namespace

void terminate() {
    terminateHandler();
    // If we get here then just abort. The terminate handler can use an
    // exception to unwind through this function, if it really wants to.
    std::abort();
}

TerminateHandler setTerminate(TerminateHandler f) noexcept {
    auto oldHandler  = terminateHandler;
    terminateHandler = f;
    return oldHandler;
}

TerminateHandler getTerminate() noexcept {
    return terminateHandler;
}
