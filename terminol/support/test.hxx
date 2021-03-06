// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef SUPPORT__TEST__HXX
#define SUPPORT__TEST__HXX

#include "terminol/support/debug.hxx"

#include <string>
#include <sstream>
#include <vector>

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
        _names.pop_back();
        ASSERT(!_names.empty(), "Too many pops.");
    }

    std::string getPath() const {
        std::string path;
        for (auto & name : _names) {
            if (!path.empty()) {
                path += "/";
            }
            path += name;
        }
        return path;
    }

public:
    explicit Test(const std::string & name) {
        _names.push_back(name);
    }

    template <typename Func, typename... Args>
    void run(const std::string & name, Func && func, Args &&...args) {
        push(name);
        func(*this, std::forward<Args>(args)...);
        pop();
    }

    bool assert(bool condition, const std::string & description) {
        return common(condition, description);
    }

    template <typename T>
    bool assertEqual(const T & lhs, const T & rhs,
                     const std::string & description) {
        auto equal = (lhs == rhs);
        std::ostringstream sst;
        sst << "(" << lhs << " == " << rhs << ") " << description;
        return common(equal, sst.str());
    }

    int rval() const {
        return _failures > 0 ? 1 : 0;
    }
};

#endif // SUPPORT__TEST__HXX
