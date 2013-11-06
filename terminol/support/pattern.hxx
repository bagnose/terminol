// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__PATTERN__HXX
#define SUPPORT__PATTERN__HXX

#include <utility>

// Inherit from this to be uncopyable (and unassignable).
class Uncopyable {
public:
    Uncopyable              ()                   = default;
    Uncopyable              (const Uncopyable &) = delete;
    Uncopyable & operator = (const Uncopyable &) = delete;
};

//
// http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
//

template <class Fun> class ScopeGuard {
    Fun  _f;
    bool _active;

public:
    explicit ScopeGuard(Fun f) : _f(std::move(f)) , _active(true) {}
    ~ScopeGuard() { if (_active) _f(); }

    void dismiss() { _active = false; }

    ScopeGuard              ()                   = delete;
    ScopeGuard              (const ScopeGuard &) = delete;
    ScopeGuard & operator = (const ScopeGuard &) = delete;

    ScopeGuard(ScopeGuard && rhs) :
        _f(std::move(rhs._f)),
        _active(rhs._active)
    {
        rhs.dismiss();
    }
};

template <class Fun> ScopeGuard<Fun> scopeGuard(Fun f) {
    return ScopeGuard<Fun>(std::move(f));
}

/*
void fun() {
    char name[] = "/tmp/deleteme.XXXXXX";
    auto fd = mkstemp(name);
    auto g1 = scopeGuard([] {
                         fclose(fd);
                         unlink(name);
                         });
    auto buf = malloc(1024 * 1024);
    auto g2 = scopeGuard([] { free(buf); });
    ...
        use fd and buf ...
}
*/

#endif // SUPPORT__PATTERN__HXX
