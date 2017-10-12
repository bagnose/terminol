// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/support/async_destroyer.hxx"
#include "terminol/support/sync_destroyer.hxx"
#include "terminol/support/test.hxx"

#include <atomic>

namespace {

template <typename Destroyer>
void destroy(Test & test) {
    std::atomic_int count(0);

    class IncDec : public I_Destroyer::Garbage {
        std::atomic_int & _count;

    public:
        IncDec(std::atomic_int & count_) : _count(count_) { ++_count; }
        ~IncDec()                                         { --_count; }
    };

    {
        ENFORCE(count.load() == 0, "Zero counter at start.");

        Destroyer destroyer;

        for (int i = 0; i != 100; ++i) {
            destroyer.add(std::make_unique<IncDec>(count));
        }
    }

    test.enforceEqual(0, count.load(), "All garbage has been destroyed.");
}

} // namespace {anonymous}

int main() {
    Test test("support/destroyer");
    test.run("async", destroy<AsyncDestroyer>);
    test.run("sync", destroy<SyncDestroyer>);

    return 0;
}
