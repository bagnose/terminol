// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/parser.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"

#include <fstream>
#include <unordered_map>

class Parser : protected Uncopyable {
    class Handler {
    public:
        virtual ~Handler() {}

        virtual void handle(const std::string & value) throw (ParseError) = 0;
    };

    typedef std::unordered_map<std::string, Handler *> Handlers;

    //

    template <class T> class SimpleHandler : public Handler {
        T & _t;
    public:
        SimpleHandler(T & t) : _t(t) {}

        void handle(const std::string & value) throw (ParseError) override {
            _t = unstringify<T>(value);
        }
    };

    //

    template <class F> class GenericHandler : public Handler {
        F _func;
    public:
        GenericHandler(F func) : _func(func) {}
        void handle(const std::string & value) throw (ParseError) override {
            _func(value);
        }
    };

    //

    typedef std::unordered_map<std::string, Action> Actions;

    //

    Config   & _config;
    Handlers   _handlers;
    Actions    _actions;

    template <class T> void registerSimpleHandler(const std::string & name, T & value) {
        auto handler = new SimpleHandler<T>(value);
        _handlers.insert(std::make_pair(name, handler));
    }

    template <class F> void registerGenericHandler(const std::string & name, F func) {
        auto handler = new GenericHandler<F>(func);
        _handlers.insert(std::make_pair(name, handler));
    }

public:
    explicit Parser(Config & config);
    ~Parser();

protected:
    void parse();
    bool tryPath(const std::string & path);
    void interpretTokens(const std::vector<std::string> & tokens) throw (ParseError);
    void handleSet(const std::string & key, const std::string & value) throw (ParseError);
    void handleBindSym(const std::string & sym, const std::string & action) throw (ParseError);
};

//
//
//

Parser::Parser(Config & config) : _config(config) {
    registerSimpleHandler("font-name", _config.fontName);

    registerSimpleHandler("font-size", _config.fontSize);
    registerSimpleHandler("term-name", _config.termName);
    registerSimpleHandler("scroll-with-history", _config.scrollWithHistory);
    registerSimpleHandler("scroll-on-tty-output", _config.scrollOnTtyOutput);
    registerSimpleHandler("scroll-on-tty-key-press", _config.scrollOnTtyKeyPress);
    registerSimpleHandler("scroll-on-resize", _config.scrollOnResize);
    registerSimpleHandler("scroll-on-paste", _config.scrollOnPaste);
    registerSimpleHandler("title", _config.title);
    registerSimpleHandler("icon", _config.icon);
    registerSimpleHandler("chdir", _config.icon);

    registerGenericHandler("scroll-back-history",
                           [&](const std::string & value)
                           {
                           _config.unlimitedScrollBack = false;
                           _config.scrollBackHistory = unstringify<size_t>(value);
                           }
                          );

    registerSimpleHandler("unlimited-scroll-back", _config.unlimitedScrollBack);
    registerSimpleHandler("frames-per-second", _config.framesPerSecond);
    registerSimpleHandler("traditional-wrapping", _config.traditionalWrapping);
    registerSimpleHandler("trace-tty", _config.traceTty);
    registerSimpleHandler("sync-tty", _config.syncTty);
    registerSimpleHandler("initial-x", _config.initialX);
    registerSimpleHandler("initial-y", _config.initialY);
    registerSimpleHandler("initial-rows", _config.initialRows);
    registerSimpleHandler("initial-cols", _config.initialCols);

    registerSimpleHandler("normal-fg-color", _config.normalFgColor);
    registerSimpleHandler("normal-bg-color", _config.normalBgColor);

    registerGenericHandler("select-fg-color",
                           [&](const std::string & value)
                           {
                           _config.customSelectFgColor = true;
                           _config.selectFgColor       = unstringify<Color>(value);
                           }
                          );

    registerGenericHandler("select-bg-color",
                           [&](const std::string & value)
                           {
                           _config.customSelectBgColor = true;
                           _config.selectBgColor       = unstringify<Color>(value);
                           }
                          );

    registerGenericHandler("cursor-fill-color",
                           [&](const std::string & value)
                           {
                           _config.customCursorFillColor = true;
                           _config.cursorFillColor      = unstringify<Color>(value);
                           }
                          );

    registerGenericHandler("cursor-text-color",
                           [&](const std::string & value)
                           {
                           _config.customCursorTextColor = true;
                           _config.cursorTextColor      = unstringify<Color>(value);
                           }
                          );

    registerSimpleHandler("scrollbar-fg-color", _config.scrollbarFgColor);
    registerSimpleHandler("scrollbar-bg-color", _config.scrollbarBgColor);

    registerSimpleHandler("scrollbar-visible", _config.scrollbarVisible);
    registerSimpleHandler("scrollbar-width", _config.scrollbarWidth);
    registerSimpleHandler("auto-hide-cursor", _config.autoHideCursor);

    registerSimpleHandler("border-color", _config.borderColor);

    registerSimpleHandler("border-thickness", _config.borderThickness);
    registerSimpleHandler("double-click-timeout", _config.doubleClickTimeout);

    registerGenericHandler("color-scheme",
                           [&](const std::string & value)
                           {
                           _config.setColorScheme(value);
                           }
                          );

    registerSimpleHandler("server-fork", _config.serverFork);

    registerSimpleHandler("color-0", _config.systemColors[0]);
    registerSimpleHandler("color-1", _config.systemColors[1]);
    registerSimpleHandler("color-2", _config.systemColors[2]);
    registerSimpleHandler("color-3", _config.systemColors[3]);
    registerSimpleHandler("color-4", _config.systemColors[4]);
    registerSimpleHandler("color-5", _config.systemColors[5]);
    registerSimpleHandler("color-6", _config.systemColors[6]);
    registerSimpleHandler("color-7", _config.systemColors[7]);
    registerSimpleHandler("color-8", _config.systemColors[8]);
    registerSimpleHandler("color-9", _config.systemColors[9]);
    registerSimpleHandler("color-10", _config.systemColors[10]);
    registerSimpleHandler("color-11", _config.systemColors[11]);
    registerSimpleHandler("color-12", _config.systemColors[12]);
    registerSimpleHandler("color-13", _config.systemColors[13]);
    registerSimpleHandler("color-14", _config.systemColors[14]);
    registerSimpleHandler("color-15", _config.systemColors[15]);

    registerSimpleHandler("cut-chars", _config.cutChars);

    registerSimpleHandler("visual-bell-color", _config.visualBellColor);

    registerSimpleHandler("visual-bell-duration", _config.visualBellDuration);
    registerSimpleHandler("map-on-bell", _config.mapOnBell);
    registerSimpleHandler("urgent-on-bell", _config.urgentOnBell);
    registerSimpleHandler("visual-bell", _config.visualBell);
    registerSimpleHandler("audible-bell", _config.audibleBell);
    registerSimpleHandler("x11-pseudo-transparency", _config.x11PseudoTransparency);
    registerSimpleHandler("x11-composited-transparency", _config.x11CompositedTransparency);
    registerSimpleHandler("x11-transparency-value", _config.x11TransparencyValue);

    //
    //
    //

    _actions.insert(std::make_pair("window-narrower",      Action::WINDOW_NARROWER));
    _actions.insert(std::make_pair("window-wider",         Action::WINDOW_WIDER));
    _actions.insert(std::make_pair("window-shorter",       Action::WINDOW_SHORTER));
    _actions.insert(std::make_pair("window-taller",        Action::WINDOW_TALLER));
    _actions.insert(std::make_pair("local-font-reset",     Action::LOCAL_FONT_RESET));
    _actions.insert(std::make_pair("local-font-smaller",   Action::LOCAL_FONT_SMALLER));
    _actions.insert(std::make_pair("local-font-bigger",    Action::LOCAL_FONT_BIGGER));
    _actions.insert(std::make_pair("global-font-reset",    Action::GLOBAL_FONT_RESET));
    _actions.insert(std::make_pair("global-font-smaller",  Action::GLOBAL_FONT_SMALLER));
    _actions.insert(std::make_pair("global-font-bigger",   Action::GLOBAL_FONT_BIGGER));
    _actions.insert(std::make_pair("copy-to-clipboard",    Action::COPY_TO_CLIPBOARD));
    _actions.insert(std::make_pair("paste-from-clipboard", Action::PASTE_FROM_CLIPBOARD));
    _actions.insert(std::make_pair("scroll-up-line",       Action::SCROLL_UP_LINE));
    _actions.insert(std::make_pair("scroll-down-line",     Action::SCROLL_DOWN_LINE));
    _actions.insert(std::make_pair("scroll-up-page",       Action::SCROLL_UP_PAGE));
    _actions.insert(std::make_pair("scroll-down-page",     Action::SCROLL_DOWN_PAGE));
    _actions.insert(std::make_pair("scroll-top",           Action::SCROLL_TOP));
    _actions.insert(std::make_pair("scroll-bottom",        Action::SCROLL_BOTTOM));
    _actions.insert(std::make_pair("clear-history",        Action::CLEAR_HISTORY));
    _actions.insert(std::make_pair("debug-global-tags",    Action::DEBUG_GLOBAL_TAGS));
    _actions.insert(std::make_pair("debug-local-tags",     Action::DEBUG_LOCAL_TAGS));
    _actions.insert(std::make_pair("debug-history",        Action::DEBUG_HISTORY));
    _actions.insert(std::make_pair("debug-active",         Action::DEBUG_ACTIVE));
    _actions.insert(std::make_pair("debug-modes",          Action::DEBUG_MODES));
    _actions.insert(std::make_pair("debug-selection",      Action::DEBUG_SELECTION));
    _actions.insert(std::make_pair("debug-stats",          Action::DEBUG_STATS));
    _actions.insert(std::make_pair("debug-stats2",         Action::DEBUG_STATS2));

    parse();
}

Parser::~Parser() {
    for (auto & t : _handlers) {
        auto handler = t.second;
        delete handler;
    }
}

void Parser::parse() {
    const std::string conf = "/terminol/config";

    auto xdg_config_home = static_cast<const char *>(::getenv("XDG_CONFIG_HOME"));

    if (xdg_config_home) {
        if (tryPath(xdg_config_home + conf)) {
            return;
        }
    }

    auto xdg_config_dirs = static_cast<const char *>(::getenv("XDG_CONFIG_DIRS"));

    if (xdg_config_dirs) {
        std::vector<std::string> dirs;
        split(xdg_config_dirs, dirs, ":");

        for (auto & d : dirs) {
            if (tryPath(d + conf)) {
                return;
            }
        }
    }

    auto home = static_cast<const char *>(::getenv("HOME"));

    if (home) {
        if (tryPath(home + std::string("/.config") + conf)) {
            return;
        }
    }

    std::cerr << "No configuration file found." << std::endl;
}

bool Parser::tryPath(const std::string & path) {
    std::ifstream ifs;
    ifs.open(path.c_str());

    if (ifs.good()) {
        size_t num = 0;
        std::string line;
        while (getline(ifs, line).good()) {
            ++num;

            std::vector<std::string> tokens;
            try {
                if (split(line, tokens)) {
                    interpretTokens(tokens);
                }
            }
            catch (const ParseError & ex) {
                std::cerr << path << ":" << num << ": " << ex.message << std::endl;
            }
        }
        return true;
    }
    else {
        return false;
    }
}

void Parser::interpretTokens(const std::vector<std::string> & tokens) throw (ParseError) {
    ASSERT(!tokens.empty(), "No tokens!");

    if (tokens[0] == "set") {
        if (tokens.size() == 3) {
            handleSet(tokens[1], tokens[2]);
        }
        else {
            throw ParseError("Syntax: 'set NAME VALUE'");
        }
    }
    else if (tokens[0] == "bindsym") {
        if (tokens.size() == 3) {
            handleBindSym(tokens[1], tokens[2]);
        }
        else {
            throw ParseError("Syntax: 'bindsym KEY ACTION'");
        }
    }
    else {
        throw ParseError("Unrecognised token: '" + tokens[0] + "'");
    }
}

void Parser::handleSet(const std::string & key, const std::string & value) throw (ParseError) {
    auto iter = _handlers.find(key);
    if (iter == _handlers.end()) {
        throw ParseError("No such setting: '" + key + "'");
    }
    else {
        auto handler = iter->second;
        handler->handle(value);
    }
}

void Parser::handleBindSym(const std::string & sym, const std::string & action) throw (ParseError) {
    std::vector<std::string> tokens;
    if (split(sym, tokens, "+")) {
        auto key = tokens.back(); tokens.pop_back();

        ModifierSet modifiers;

        for (auto & m : tokens) {
            auto modifier = xkb::nameToModifier(m);
            modifiers.set(modifier);
        }

        auto keySym = xkb::nameToSym(key);

        KeyCombo keyCombo(keySym, modifiers);

        auto iter = _actions.find(action);
        if (iter == _actions.end()) {
            throw ParseError("Bad action: '" + action + "'");
        }
        else {
            auto resolvedAction = iter->second;
            _config.bindings.insert(std::make_pair(keyCombo, resolvedAction));
        }
    }
    else {
        throw ParseError("??");
    }
}

//
//
//

void parseConfig(Config & config) {
    Parser parser(config);
}
