#ifndef SUPPORT__ASYNC_INVOKER__HXX
#define SUPPORT__ASYNC_INVOKER__HXX

#include "terminol/support/pattern.hxx"
#include "terminol/support/queue.hxx"

#include <thread>

class AsyncInvoker final {
    using Function      = std::function<void()>;
    using FunctionQueue = Queue<Function>;

    FunctionQueue _queue;
    std::thread   _thread;

public:
    AsyncInvoker() : _thread(&AsyncInvoker::background, this) {}

    ~AsyncInvoker() {
        _queue.finalise();
        _thread.join();
    }

    // Callable from multiple threads
    void add(const Function & function) { _queue.add(function); }

private:
    void background() {
        while (auto function = _queue.remove()) { (*function)(); }
    }
};

#endif // SUPPORT__ASYNC_INVOKER__HXX
