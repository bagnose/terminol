// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__DESTROYER_INTERFACE__HXX
#define SUPPORT__DESTROYER_INTERFACE__HXX

#include <memory>

class I_Destroyer {
public:
    class Garbage {
    public:
        virtual ~Garbage() = 0;       // Heavy lifting goes here.
    };

    virtual void add(std::unique_ptr<Garbage> garbage) = 0;

protected:
    ~I_Destroyer() = default;
};

inline I_Destroyer::Garbage::~Garbage() = default;

#endif // SUPPORT__DESTROYER_INTERFACE__HXX
