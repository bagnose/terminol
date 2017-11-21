// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__PATTERN__HXX
#define SUPPORT__PATTERN__HXX

#include "terminol/support/debug.hxx"

#include <utility>
#include <functional>
#include <memory>

// Inherit from this to be uncopyable (and unassignable).
class Uncopyable {
public:
    Uncopyable(const Uncopyable &) noexcept = delete;
    Uncopyable & operator=(const Uncopyable &) noexcept = delete;

protected:
    Uncopyable() noexcept  = default;
    ~Uncopyable() noexcept = default;
};

//
// http://channel9.msdn.com/Shows/Going+Deep/C-and-Beyond-2012-Andrei-Alexandrescu-Systematic-Error-Handling-in-C
//

class ScopeGuard final : private Uncopyable {
    using Function = std::function<void()>;

    Function _function;

public:
    explicit ScopeGuard(Function function) : _function(std::move(function)) { ASSERT(_function, ); }

    ScopeGuard(ScopeGuard && scope_guard) noexcept
        : _function(std::exchange(scope_guard._function, nullptr)) {}

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

// Adapter class that presents a container in reverse.
// Use the convenience function reversed() to instantiate.
template <typename Container>
class Reverser {
    Container & _container;

public:
    explicit Reverser(Container & container) throw() : _container(container) {}

    // Use 'auto' for return value deduction because the type isn't interesting.
    auto begin() const throw() { return _container.rbegin(); }
    auto end() const throw() { return _container.rend(); }
};

// Example usage:
//
// for (auto & i : reversed(container)) {
//     // use i
// }
template <typename Container>
Reverser<Container> reversed(Container & container) throw() {
    return Reverser<Container>(container);
}

// Adapter class that presents a const container in reverse.
// Use the convenience function reversed() to instantiate.
template <typename Container>
class ConstReverser {
    const Container & _container;

public:
    explicit ConstReverser(const Container & container) throw() : _container(container) {}

    // Use 'auto' for return value deduction because the type isn't interesting.
    auto begin() const throw() { return _container.rbegin(); }
    auto end() const throw() { return _container.rend(); }
};

// Example usage:
//
// for (auto & i : reversed(container)) {
//     // use i
// }
template <typename Container>
ConstReverser<Container> reversed(const Container & container) throw() {
    return ConstReverser<Container>(container);
}

// Use this type when applying the Pimpl idiom, e.g.:
//
// .h:
//
//   class Foo {
//   public:
//       Foo(int a, bool b) throw ();
//       Foo(Foo &&) throw ();
//       ~Foo() throw ();
//
//   private:
//       class Private;
//       PimplPtr<Private> _private;
//   };
//
// .cc:
//
//   class Foo::Private {
//       Private(int a, bool b) {}
//   };
//
//   Foo::Foo(int a, bool b) : _private(a, b)
//
//   Foo::Foo(Foo &&) = default;
//
//   Foo::~Foo() = default;
//
// This class is superior to using std::unique_ptr in the Pimpl idiom because:
//
// - it provides exactly what is necessary, and nothing more
// - its constructor forces immediate construction of the pointer,
//   without the need to re-state the type name.
template <typename T>
class PimplPtr final {
    std::unique_ptr<T> mPtr;

public:
    template <typename... Ts>
    explicit PimplPtr(Ts &&... params) : mPtr(std::make_unique<T>(std::forward<Ts>(params)...)) {}

    T * operator->() throw() { return mPtr.operator->(); }
    T & operator*() throw() { return mPtr.operator*(); }

    const T * operator->() const throw() { return mPtr.operator->(); }
    const T & operator*() const throw() { return mPtr.operator*(); }
};

#endif // SUPPORT__PATTERN__HXX
