// vi:noai:sw=4

#ifndef SUPPORT__PATTERN__HXX
#define SUPPORT__PATTERN__HXX

#include <utility>

// Inherit from this to be uncopyable.
class Uncopyable {
public:
    Uncopyable() {}

    Uncopyable              (const Uncopyable &) = delete;
    Uncopyable & operator = (const Uncopyable &) = delete;
};

// 
// http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
//

template <class Fun>
class ScopeGuard {
    Fun _f;
    bool _active;

public:
    ScopeGuard(Fun f) : _f(std::move(f)) , _active(true) {}
    ~ScopeGuard() { if (_active) _f(); }
    void dismiss() { _active = false; }
    ScopeGuard              ()                   = delete;      // XXX necessary?
    ScopeGuard              (const ScopeGuard &) = delete;
    ScopeGuard & operator = (const ScopeGuard &) = delete;
    ScopeGuard(ScopeGuard&& rhs)
        : _f(std::move(rhs._f))
          , _active(rhs._active) {
              rhs.dismiss();
          }
};

template <class Fun>
ScopeGuard<Fun> scopeGuard(Fun f) {
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

#if 0
namespace detail {
    enum class ScopeGuardOnExit {};

    template <typename Fun>
    ScopeGuard<Fun> operator + (ScopeGuardOnExit, Fun&& fn) {
        return ScopeGuard<Fun>(std::forward<Fun>(fn));
    }
} // namespace detail

#define SCOPE_EXIT \
    auto ANONYMOUS_VARIABLE(SCOPE_EXIT_STATE) \
    = ::detail::ScopeGuardOnExit() + [&]()


#define CONCATENATE_IMPL(s1, s2) s1##s2
#define CONCATENATE(s1, s2) CONCATENATE_IMPL(s1, s2)
#ifdef __COUNTER__
#define ANONYMOUS_VARIABLE(str) \
    CONCATENATE(str, __COUNTER__)
#else
#define ANONYMOUS_VARIABLE(str) \
    CONCATENATE(str, __LINE__)
#endif

/*
void fun() {
    char name[] = "/tmp/deleteme.XXXXXX";
    auto fd = mkstemp(name);
    SCOPE_EXIT { fclose(fd); unlink(name); };
    auto buf = malloc(1024 * 1024);
    SCOPE_EXIT { free(buf); };

    // use fd and buf ...
}
*/
#endif

#endif // SUPPORT__PATTERN__HXX
