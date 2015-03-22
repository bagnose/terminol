// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/queue.hxx"

#include <thread>
#include <vector>
#include <algorithm>

template <typename T>
class Consumer {
    Queue<T>  & _queue;
    std::thread _thread;

    void consume() {
        try {
            for (;;) {
                _queue.remove();
                std::cout << static_cast<void *>(this) << " consumed" << std::endl;
            }
        }
        catch (const typename Queue<T>::Finalised &) {
        }
    }

public:
    Consumer(Queue<T> & queue) :
        _queue(queue),
        _thread(&Consumer::consume, this) {}

    ~Consumer() {
        _thread.join();
    }
};

template <typename T>
class Producer {
    Queue<T>  & _queue;
    std::thread _thread;

    void produce() {
        for (int i = 0; i != 10; ++i) {
            _queue.add(T());
            std::cout << static_cast<void *>(this) << " produced" << std::endl;
        }
    }

public:
    Producer(Queue<T> & queue) :
        _queue(queue),
        _thread(&Producer::produce, this) {}

    ~Producer() {
        _thread.join();
    }
};

int main() {
    typedef int Item;

    Queue<Item> queue;

    {
        Consumer<Item> consumer1(queue);
        Consumer<Item> consumer2(queue);
        Consumer<Item> consumer3(queue);

        {
            Producer<Item> producer1(queue);
            Producer<Item> producer2(queue);
            Producer<Item> producer3(queue);
        }

        {
            Producer<Item> producer1(queue);
            Producer<Item> producer2(queue);
            Producer<Item> producer3(queue);
        }

        queue.finalise();
    }

    return 0;
}
