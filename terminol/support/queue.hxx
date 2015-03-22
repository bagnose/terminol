// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__QUEUE__HXX
#define SUPPORT__QUEUE__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/support/debug.hxx"

#include <queue>
#include <mutex>
#include <condition_variable>

template <typename T>
class Queue : private Uncopyable {
public:
    struct Finalised {};

    Queue() : _queue(), _finalised(false), _mutex(), _condition() {}

    ~Queue() {
        finalise();
    }

    // These two functions are called by the producer:

    void add(T && t) {
        std::unique_lock<std::mutex> lock(_mutex);
        ASSERT(!_finalised, "Add after finalised.");
        _queue.push(t);
        _condition.notify_one();
    }

    void finalise() {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_finalised) {
            _finalised = true;
            _condition.notify_all();
        }
    }

    // This function is called by the consumer:

    T remove() throw (Finalised) {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this]{ return _finalised || !_queue.empty(); });

        if (!_queue.empty()) {
            auto t = std::move(_queue.front());
            _queue.pop();
            return t;
        }

        ASSERT(_finalised, "");
        throw Finalised();
    }

private:
    std::queue<T>           _queue;
    bool                    _finalised;
    std::mutex              _mutex;
    std::condition_variable _condition;
};

#endif // SUPPORT__QUEUE__HXX
