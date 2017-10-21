// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__ASYNC_DESTROYER__HXX
#define SUPPORT__ASYNC_DESTROYER__HXX

#include "terminol/support/destroyer_interface.hxx"
#include "terminol/support/pattern.hxx"
#include "terminol/support/queue.hxx"

#include <thread>

class AsyncDestroyer final : public I_Destroyer, private Uncopyable {
public:
    AsyncDestroyer() : _thread(&AsyncDestroyer::background, this) {}

    ~AsyncDestroyer() {
        _queue.finalise();
        _thread.join();
    }

    // Callable from multiple threads.
    void add(std::unique_ptr<Garbage> garbage) override {
        _queue.add(std::move(garbage));
    }

protected:
    void background() {
        while (_queue.remove()) {}
    }

private:
    using GarbageQueue = Queue<std::unique_ptr<Garbage>>;

    GarbageQueue _queue;
    std::thread  _thread;
};

#endif // SUPPORT__ASYNC_DESTROYER__HXX
