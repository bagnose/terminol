#ifndef XCB__EXCEPTION__H
#define XCB__EXCEPTION__H

#include "terminol/support/exception.hxx"

class XError final : public Exception {
public:
    explicit XError(const std::string & what_arg) : Exception(what_arg) {}
};

#endif // XCB__EXCEPTION__H
