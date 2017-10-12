// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__TEST__HXX
#define SUPPORT__TEST__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

#include <string>
#include <sstream>
#include <vector>
#include <exception>
#include <iterator>

class Test {
    std::vector<std::string> _names;
    int                      _count     = 0;
    int                      _successes = 0;
    int                      _failures  = 0;

    bool common(bool success, const std::string & message);

    void push(const std::string & name) {
        _names.push_back(name);
    }

    void pop() {
        ASSERT(!_names.empty(), "Too many pops.");
        _names.pop_back();
    }

    std::string getPath() const {
        std::ostringstream ost;
        std::copy(_names.begin(), _names.end(),
                  std::ostream_iterator<std::string>(ost, "/"));
        return ost.str();
    }

    template <typename T>
    static std::string stringify(const T & t, bool abbrev) {
        auto str = ::stringify(t);

        if (abbrev) {
            constexpr size_t MAX = 16;
            if (str.size() > MAX) {
                str.resize(MAX);
                std::fill(str.end() - 3, str.end(), '.');
            }
        }

        return str;
    }

public:
    explicit Test(const std::string & name) {
        push(name);
    }

    ~Test() {
        if (!std::uncaught_exception()) {
            pop();
            ASSERT(_names.empty(), "Insufficient pops");

            if (_failures > 0) {
                std::terminate();
            }
        }
    }

    template <typename Func, typename... Args>
    void run(const std::string & name, Func && func, Args &&...args) {
        push(name);
        func(*this, std::forward<Args>(args)...);
        pop();
    }

    bool enforce(bool condition, const std::string & description) {
        return common(condition, description);
    }

    template <typename T>
    bool enforceEqual(const T & lhs, const T & rhs,
                      const std::string & description) {
        auto equal = (lhs == rhs);
        std::ostringstream sst;
        sst << "("
            << stringify(lhs, equal)
            << " == "
            << stringify(rhs, equal)
            << ") " << description;
        return common(equal, sst.str());
    }
};

#endif // SUPPORT__TEST__HXX
