// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__ASYNC_DESTROYER__HXX
#define SUPPORT__ASYNC_DESTROYER__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/support/debug.hxx"
#include "terminol/support/queue.hxx"

#include <thread>

class AsyncDestroyer : private Uncopyable {
public:
    class Garbage {
    public:
        virtual ~Garbage() = default;       // Heavy lifting goes here.
    protected:
        Garbage() = default;
    };

    //
    //
    //

    AsyncDestroyer() : _queue(), _thread(&AsyncDestroyer::background, this) {}

    ~AsyncDestroyer() {
        _queue.finalise();
        _thread.join();
    }

    // Callable from multiple threads.
    void add(Garbage * garbage) {
        _queue.add(std::move(garbage));
    }

protected:
    void background() {
        try {
            for (;;) {
                auto garbage = _queue.remove();
                delete garbage;     // Do the heavy lifting.
            }
        }
        catch (const Queue<Garbage *>::Finalised &) {
        }
    }

private:
    Queue<Garbage *> _queue;
    std::thread      _thread;
};

#endif // SUPPORT__ASYNC_DESTROYER__HXX
