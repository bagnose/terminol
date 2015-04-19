// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT_DESTROYER_INTERFACE__HXX
#define SUPPORT_DESTROYER_INTERFACE__HXX

class I_Destroyer {
public:
    class Garbage {
    public:
        virtual ~Garbage() = default;       // Heavy lifting goes here.
    protected:
        Garbage() = default;
    };

    virtual void add(Garbage * garbage) = 0;

protected:
    ~I_Destroyer() {}
};

#endif // SUPPORT_DESTROYER_INTERFACE__HXX
