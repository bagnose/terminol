// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/async_destroyer.hxx"

#include <atomic>

class IncDec : public AsyncDestroyer::Garbage {
    std::atomic_int & _count;

public:
    IncDec(std::atomic_int & count) : _count(count) { ++_count; }
    ~IncDec() override                              { --_count; }
};

int main() {
    std::atomic_int count(0);

    {
        ENFORCE(count.load() == 0, "Zero counter at start.");

        AsyncDestroyer async_destroyer;

        for (int i = 0; i != 100; ++i) {
            async_destroyer.add(new IncDec(count));
        }
    }

    ENFORCE(count.load() == 0, "All garbage has been destroyed.");

    return 0;
}
