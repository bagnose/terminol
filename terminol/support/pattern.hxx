// vi:noai:sw=4

#ifndef SUPPORT__PATTERN__HXX
#define SUPPORT__PATTERN__HXX

// Inherit from this to be uncopyable.
class Uncopyable {
public:
    Uncopyable() {}

    Uncopyable              (const Uncopyable &) = delete;
    Uncopyable & operator = (const Uncopyable &) = delete;
};

#endif // SUPPORT__PATTERN__HXX
