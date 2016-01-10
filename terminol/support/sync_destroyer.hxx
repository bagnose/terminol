// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__SYNC_DESTROYER__HXX
#define SUPPORT__SYNC_DESTROYER__HXX

#include "terminol/support/destroyer_interface.hxx"
#include "terminol/support/pattern.hxx"

class SyncDestroyer : public I_Destroyer, private Uncopyable {
public:
    virtual ~SyncDestroyer() = default;

    void add(std::unique_ptr<Garbage> garbage) override { garbage.reset(); }
};

#endif // SUPPORT__SYNC_DESTROYER__HXX
