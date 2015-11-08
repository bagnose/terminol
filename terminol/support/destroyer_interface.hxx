// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__DESTROYER_INTERFACE__HXX
#define SUPPORT__DESTROYER_INTERFACE__HXX

#include <memory>

class I_Destroyer {
public:
    class Garbage {
    public:
        virtual ~Garbage() = default;       // Heavy lifting goes here.

    protected:
        Garbage() = default;
    };

    virtual void add(std::unique_ptr<Garbage> garbage) = 0;

protected:
    ~I_Destroyer() {}
};

#endif // SUPPORT__DESTROYER_INTERFACE__HXX
