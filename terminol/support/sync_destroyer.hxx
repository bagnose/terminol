// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__SYNC_DESTROYER__HXX
#define SUPPORT__SYNC_DESTROYER__HXX

#include "terminol/support/destroyer_interface.hxx"
#include "terminol/support/pattern.hxx"

class SyncDestroyer : public I_Destroyer, private Uncopyable {
public:
    virtual ~SyncDestroyer() {}

    void add(Garbage * garbage) override {
        delete garbage;
    }
};

#endif // SUPPORT__SYNC_DESTROYER__HXX
