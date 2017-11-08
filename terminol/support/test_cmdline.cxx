// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/cmdline.hxx"
#include "terminol/support/debug.hxx"

#include <cstdarg>

namespace {

void parse(CmdLine * cmdLine, ...) {
    va_list ap;
    std::vector<const char *> vector;

    va_start(ap, cmdLine);

    for (;;) {
        auto value = va_arg(ap, const char *);
        if (!value) {
            break;
        }
        vector.push_back(value);
    }

    va_end(ap);

    cmdLine->parse(vector.size(), &vector.front());
}

} // namespace {anonymous}

int main() {
    {
        CmdLine cmdLine("help", "version");
        bool goTrue  = false;
        bool goFalse = true;
        cmdLine.add(std::make_unique<BoolHandler>(goTrue),  '\0', "bool1", true);
        cmdLine.add(std::make_unique<BoolHandler>(goFalse), '\0', "bool2", true);
        parse(&cmdLine, "dummy", "--bool1", "--no-bool2", nullptr);
        ENFORCE(goTrue, );
        ENFORCE(!goFalse, );
    }

    {
        CmdLine cmdLine("help", "version");
        std::string str;
        cmdLine.add(std::make_unique<IStreamHandler<std::string>>(str), '\0', "str", true);
        parse(&cmdLine, "dummy", "--str=foo", nullptr);
        ENFORCE(str == "foo", );
    }

    return 0;
}
