// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__QUEUE__HXX
#define SUPPORT__QUEUE__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/support/debug.hxx"

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template <typename T>
class Queue : private Uncopyable {
public:
    // These functions are called by the producer:

    void add(const T & t) {
        std::unique_lock<std::mutex> lock(_mutex);
        ASSERT(!_finalised, "Add after finalised.");
        _queue.push(t);
        _condition.notify_one();
    }

    void add(T && t) {
        std::unique_lock<std::mutex> lock(_mutex);
        ASSERT(!_finalised, "Add after finalised.");
        _queue.push(std::move(t));
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

    std::optional<T> remove() {
        std::unique_lock<std::mutex> lock(_mutex);
        _condition.wait(lock, [this]() { return _finalised || !_queue.empty(); });

        if (_queue.empty()) {
            return std::nullopt;
        }
        else {
            auto t = std::move(_queue.front());
            _queue.pop();
            return t;
        }
    }

private:
    std::queue<T>           _queue;
    std::mutex              _mutex;
    std::condition_variable _condition;
    bool                    _finalised = false;
};

#endif // SUPPORT__QUEUE__HXX
