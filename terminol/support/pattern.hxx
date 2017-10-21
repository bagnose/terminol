// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__PATTERN__HXX
#define SUPPORT__PATTERN__HXX

#include "terminol/support/debug.hxx"

#include <utility>
#include <functional>

// Inherit from this to be uncopyable (and unassignable).
class Uncopyable {
public:
    Uncopyable(const Uncopyable &) noexcept = delete;
    Uncopyable & operator = (const Uncopyable &) noexcept = delete;

protected:
    Uncopyable() noexcept = default;
    ~Uncopyable() noexcept = default;
};

//
// http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
//

class ScopeGuard : private Uncopyable {
    using Function = std::function<void ()>;

    Function _function;

public:
    explicit ScopeGuard(Function function) : _function(std::move(function)) { ASSERT(_function, ""); }

    ScopeGuard(ScopeGuard && scope_guard) noexcept :
        _function(std::exchange(scope_guard._function, nullptr))
    {}

    ~ScopeGuard() {
        if (_function) { _function(); }
    }

    // Prevent the guard from executing when it is destroyed.
    void dismiss() { _function = nullptr; }
};

/*
void fun() {
    char name[] = "/tmp/deleteme.XXXXXX";
    auto fd = mkstemp(name);
    auto g1 = scopeGuard([] {
                         fclose(fd);
                         unlink(name);
                         });
    auto buf = malloc(1024 * 1024);
    ScopeGuard guard([]() { std::free(buf); });
    ...
        use fd and buf ...
}
*/

#endif // SUPPORT__PATTERN__HXX
