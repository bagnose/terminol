// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/cmdline.hxx"
#include "terminol/support/debug.hxx"

#include <cstdarg>

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

int main() {
    try {
        CmdLine cmdLine("help", "version");
        bool goTrue  = false;
        bool goFalse = true;
        cmdLine.add(new BoolHandler(goTrue),  '\0', "bool1", true);
        cmdLine.add(new BoolHandler(goFalse), '\0', "bool2", true);
        parse(&cmdLine, "dummy", "--bool1", "--no-bool2", nullptr);
        ENFORCE(goTrue,   "");
        ENFORCE(!goFalse, "");
    }
    catch (const CmdLine::Error & error) {
        std::cerr << error.message << std::endl;
    }

    try {
        CmdLine cmdLine("help", "version");
        std::string str;
        cmdLine.add(new IStreamHandler<std::string>(str), '\0', "str", true);
        parse(&cmdLine, "dummy", "--str=foo", nullptr);
        ENFORCE(str == "foo", "");
    }
    catch (const CmdLine::Error & error) {
        std::cerr << error.message << std::endl;
    }

    return 0;
}
