// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/support/debug.hxx"
#include "terminol/support/conv.hxx"

#include <string>
#include <map>
#include <cstdlib>
#include <memory>
#include <functional>

// Option, value

class CmdLine {
public:
    class Handler {
    public:
        virtual bool isNegatable() const = 0;
        virtual bool wantsValue() const = 0;
        virtual void handle(bool negated, const std::string & value) = 0;

        virtual ~Handler() {}
    };

private:
    struct Option {
        Option(Handler           * handler_,
               char                shortOpt_,
               const std::string & longOpt_,
               bool                mandatory_) :
            handler(handler_),
            shortOpt(shortOpt_),
            longOpt(longOpt_),
            mandatory(mandatory_)
        {}

        Handler     * handler;
        char          shortOpt;
        std::string   longOpt;
        bool          mandatory;
        bool          serviced = false;
    };

    std::string                      _help;
    std::string                      _version;
    std::string                      _delimiter;

    std::vector<Option>              _options;

    std::map<char,        Handler *> _shortToHandler;
    std::map<std::string, Handler *> _longToHandler;

public:
    CmdLine(const std::string & help,
            const std::string & version,
            const std::string & delimiter = "--") :
        _help(help),
        _version(version),
        _delimiter(delimiter) {}

    ~CmdLine() {
        for (auto & o : _options) {
            delete o.handler;
        }
    }

    void add(Handler * handler,
             char shortOpt,
             const std::string & longOpt,
             bool mandatory = false) {
        ENFORCE(!longOpt.empty() || shortOpt != '\0', "");

        if (!longOpt.empty()) {
            ENFORCE(_longToHandler.find(longOpt) == _longToHandler.end(), "");
            _longToHandler.insert(std::make_pair(longOpt, handler));
        }

        if (shortOpt != '\0') {
            ENFORCE(_shortToHandler.find(shortOpt) == _shortToHandler.end(), "");
            _shortToHandler.insert(std::make_pair(shortOpt, handler));
        }

        _options.push_back(Option(handler, shortOpt, longOpt, mandatory));
    }

    std::vector<std::string> parse(int argc, const char ** argv) {
        std::vector<std::string> arguments;
        bool ignore = false;

        ASSERT(argc >= 1, "");
        for (int i = 1; i != argc; ++i) {
            std::string str = argv[i];

            if (str == "--help") {
                std::cerr << _help;
                std::exit(0);
            }

            if (str == "--version") {
                std::cerr << "Terminol version " << _version;
                std::exit(0);
            }

            if (str == _delimiter) {
                ignore = true;
                continue;
            }

            if (ignore || str.empty() || str.front() != '-') {
                arguments.push_back(str);
            }
            else if (str == "--") {
                ignore = true;
            }
            else {
                // It must be an option
                if (str.substr(0, 2) == "--") {
                    size_t j = 2;
                    std::string value;
                    bool negated = false;

                    if (str.substr(j, 3) == "no-") {
                        negated = true;
                        j += 3;
                    }

                    // Long option.
                    auto e = str.find('=', j);

                    if (e == std::string::npos) {
                        // No value here
                        auto opt = str.substr(j);
                        auto handler = lookupLong(opt);
                        if (handler->wantsValue()) {
                            ++i;
                            if (i == argc) {
                                THROW(ConversionError("No value provided"));
                            }
                            value = argv[i];
                            handler->handle(negated, value);
                        }
                        else {
                            handler->handle(negated, value);
                        }
                    }
                    else {
                        auto opt = str.substr(j, e - j);
                        j = e + 1;
                        value = str.substr(j);
                        auto handler = lookupLong(opt);
                        if (handler->wantsValue()) {
                            handler->handle(negated, value);
                        }
                        else {
                            THROW(ConversionError("No value required for option."));
                        }
                    }
                }
                else if (str.substr(0, 1) == "-") {
                    // Short option.
                    std::string value;
                    bool negated = false;

                    for (size_t j = 1; j != str.size(); ++j) {
                        auto handler = lookupShort(str[j]);
                        if (handler->wantsValue()) {
                            // TODO
                        }
                        else {
                            handler->handle(negated, value);
                        }
                    }
                }
                else {
                    ENFORCE(false, "Unreachable");
                }
            }
        }

        return arguments;
    }

protected:
    Handler * lookupShort(char s) {
        auto iter = _shortToHandler.find(s);

        if (iter == _shortToHandler.end()) {
            std::string str; str.push_back(s);
            THROW(UserError("Unknown option: -" + str));
        }

        return iter->second;
    }

    Handler * lookupLong(const std::string & l) {
        auto iter = _longToHandler.find(l);

        if (iter == _longToHandler.end()) {
            THROW(UserError("Unknown option: --" + l));
        }

        return iter->second;
    }
};

//
//
//

class BoolHandler : public CmdLine::Handler {
    bool & _value;
public:
    BoolHandler(bool & value) : _value(value) {}

    bool isNegatable() const override { return true; }
    bool wantsValue()  const override { return false; }

    void handle(bool negated, const std::string & UNUSED(value)) override {
        _value = !negated;
    }
};

template <class V>
class IStreamHandler final : public CmdLine::Handler {
    V & _value;
public:
    IStreamHandler(V & value) : _value(value) {}

    bool isNegatable() const override { return false; }
    bool wantsValue()  const override { return true; }

    void handle(bool UNUSED(negated), const std::string & value) override {
        _value = unstringify<V>(value);
    }
};

using StringHandler = IStreamHandler<std::string>;
using IntHandler    = IStreamHandler<int>;

class MiscHandler final : public CmdLine::Handler {
    using Function = std::function<void (const std::string &)>;
    Function _func;
public:
    explicit MiscHandler(const Function & func) : _func(func) {}

    bool isNegatable() const override { return false; }
    bool wantsValue()  const override { return true; }

    void handle(bool UNUSED(negated), const std::string & value) override {
        _func(value);
    }
};
