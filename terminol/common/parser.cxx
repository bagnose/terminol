// vi:noai:sw=4

#include "terminol/common/parser.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"

#include <fstream>

namespace {

bool open(std::ifstream & ifs, const std::string & path) {
    ifs.open(path.c_str());
    return ifs.good();
}

std::string lowercase(const std::string & str) {
    auto copy = str;
    for (auto & c : copy) { c = tolower(c); }
    return copy;
}

void handleSet(const std::string & key,
               const std::string & value,
               Config & config) throw (ParseError) {
    if (key == "font-name") {
        config.fontName = value;
    }
    else if (key == "font-size") {
        config.fontSize = unstringify<int>(value);
    }
    else if (key == "term-name") {
        config.termName = value;
    }
    else if (key == "scroll-with-history") {
        config.scrollWithHistory = unstringify<bool>(value);
    }
    else if (key == "scroll-on-tty-output") {
        config.scrollOnTtyOutput = unstringify<bool>(value);
    }
    else if (key == "scroll-on-tty-key-press") {
        config.scrollOnTtyKeyPress = unstringify<bool>(value);
    }
    else if (key == "scroll-on-resize") {
        config.scrollOnResize = unstringify<bool>(value);
    }
    else if (key == "scroll-on-paste") {
        config.scrollOnPaste = unstringify<bool>(value);
    }
    else if (key == "title") {
        config.title = value;
    }
    else if (key == "icon") {
        config.icon = value;
    }
    else if (key == "chdir") {
        config.icon = value;
    }
    else if (key == "scroll-back-history") {
        config.scrollBackHistory = unstringify<size_t>(value);
    }
    else if (key == "unlimited-scroll-back") {
        config.unlimitedScrollBack = unstringify<bool>(value);
    }
    else if (key == "frames-per-second") {
        config.framesPerSecond = unstringify<int>(value);
    }
    else if (key == "traditional-wrapping") {
        config.traditionalWrapping = unstringify<bool>(value);
    }
    else if (key == "trace-tty") {
        config.traceTty = unstringify<bool>(value);
    }
    else if (key == "sync-tty") {
        config.syncTty = unstringify<bool>(value);
    }
    else if (key == "initial-x") {
        config.initialX = unstringify<uint16_t>(value);
    }
    else if (key == "initial-y") {
        config.initialY = unstringify<uint16_t>(value);
    }
    else if (key == "initial-rows") {
        config.initialRows = unstringify<uint16_t>(value);
    }
    else if (key == "initial-cols") {
        config.initialCols = unstringify<uint16_t>(value);
    }
    else if (key == "normal-fg-color") {
        config.normalFgColor = Color::fromString(value);
    }
    else if (key == "normal-bg-color") {
        config.normalBgColor = Color::fromString(value);
    }
    // systemColors[16]
    else if (key == "select-bg-color") {
        config.customSelectBgColor = true;
        config.selectBgColor = Color::fromString(value);
    }
    else if (key == "select-fg-color") {
        config.customSelectFgColor = true;
        config.selectFgColor = Color::fromString(value);
    }
    else if (key == "cursor-fill-color") {
        config.customCursorFillColor = true;
        config.cursorFillColor = Color::fromString(value);
    }
    else if (key == "cursor-text-color") {
        config.customCursorTextColor = true;
        config.cursorTextColor = Color::fromString(value);
    }
    else if (key == "scrollbar-fg-color") {
        config.scrollbarFgColor = Color::fromString(value);
    }
    else if (key == "scrollbar-bg-color") {
        config.scrollbarBgColor = Color::fromString(value);
    }
    else if (key == "scrollbar-width") {
        config.scrollbarWidth = unstringify<int>(value);
    }
    else if (key == "border-color") {
        config.borderColor = Color::fromString(value);
    }
    else if (key == "border-thickness") {
        config.borderThickness = unstringify<int>(value);
    }
    else if (key == "double-click-timeout") {
        config.doubleClickTimeout = unstringify<uint32_t>(value);
    }
    else if (key == "color-scheme") {
        config.setColorScheme(value);
    }
    else if (key == "server-fork") {
        config.serverFork = unstringify<bool>(value);
    }
    else {
        throw ParseError("No such setting: '" + key + "'");
    }
}

Action lookupAction(const std::string & str) throw (ParseError) {
    if (str == "local-font-reset") {
        return Action::LOCAL_FONT_RESET;
    }
    else if (str == "local-font-smaller") {
        return Action::LOCAL_FONT_SMALLER;
    }
    else if (str == "local-font-bigger") {
        return Action::LOCAL_FONT_BIGGER;
    }
    else if (str == "global-font-reset") {
        return Action::GLOBAL_FONT_RESET;
    }
    else if (str == "global-font-smaller") {
        return Action::GLOBAL_FONT_SMALLER;
    }
    else if (str == "global-font-bigger") {
        return Action::GLOBAL_FONT_BIGGER;
    }
    else if (str == "copy-to-clipboard") {
        return Action::COPY_TO_CLIPBOARD;
    }
    else if (str == "paste-from-clipboard") {
        return Action::PASTE_FROM_CLIPBOARD;
    }
    else if (str == "scroll-up-line") {
        return Action::SCROLL_UP_LINE;
    }
    else if (str == "scroll-down-line") {
        return Action::SCROLL_DOWN_LINE;
    }
    else if (str == "scroll-up-page") {
        return Action::SCROLL_UP_PAGE;
    }
    else if (str == "scroll-down-page") {
        return Action::SCROLL_DOWN_PAGE;
    }
    else if (str == "scroll-top") {
        return Action::SCROLL_TOP;
    }
    else if (str == "scroll-bottom") {
        return Action::SCROLL_BOTTOM;
    }
    else if (str == "clear-history") {
        return Action::CLEAR_HISTORY;
    }
    else if (str == "debug-global-tags") {
        return Action::DEBUG_GLOBAL_TAGS;
    }
    else if (str == "debug-local-tags") {
        return Action::DEBUG_LOCAL_TAGS;
    }
    else if (str == "debug-history") {
        return Action::DEBUG_HISTORY;
    }
    else if (str == "debug-active") {
        return Action::DEBUG_ACTIVE;
    }
    else if (str == "debug-modes") {
        return Action::DEBUG_MODES;
    }
    else if (str == "debug-selection") {
        return Action::DEBUG_SELECTION;
    }
    else if (str == "debug-stats") {
        return Action::DEBUG_STATS;
    }
    else if (str == "debug-stats2") {
        return Action::DEBUG_STATS2;
    }
    else {
        throw ParseError("Bad action: '" + str + "'");
    }
}

void handleBindSym(const std::string & sym,
                   const std::string & action,
                   Config & config) throw (ParseError) {
    //PRINT("sym=" << sym << ", action=" << action);

    std::vector<std::string> tokens;
    if (split(sym, tokens, "+")) {
        auto key = tokens.back(); tokens.pop_back();

        ModifierSet modifiers;

        for (const auto & m : tokens) {
            auto modifier = xkb::nameToModifier(m);
            modifiers.set(modifier);
        }

        auto keySym = xkb::nameToSym(key);

        KeyCombo keyCombo(keySym, modifiers);
        Action   action2 = lookupAction(action);

        //PRINT("Bound: " << modifiers << "-" << xkb::symToName(keySym) << " to " << action);

        config.bindings.insert(std::make_pair(keyCombo, action2));
    }
}

void interpretTokens(const std::vector<std::string> & tokens,
                     Config & config) throw (ParseError) {
    ASSERT(!tokens.empty(), "No tokens!");

    if (tokens[0] == "set") {
        if (tokens.size() == 3) {
            handleSet(tokens[1], tokens[2], config);
        }
        else {
            throw ParseError("Syntax: 'set NAME VALUE'");
        }
    }
    else if (tokens[0] == "bindsym") {
        if (tokens.size() == 3) {
            handleBindSym(tokens[1], tokens[2], config);
        }
        else {
            throw ParseError("Syntax: 'bindsym KEY ACTION'");
        }
    }
    else {
        throw ParseError("Unrecognised token: '" + tokens[0] + "'");
    }
}

void readLines(std::istream & ist, Config & config) throw (ParseError) {
    size_t num = 0;
    std::string line;
    while (getline(ist, line).good()) {
        ++num;

        std::vector<std::string> tokens;
        try {
            if (split(line, tokens)) {
                interpretTokens(tokens, config);
            }
        }
        catch (const ParseError & ex) {
            std::ostringstream ost;
            ost << num << ": ";
            throw ParseError(ost.str() + ex.message);
        }
    }
}

bool tryConfig(const std::string & path, Config & config) {
    std::ifstream ifs;

    if (open(ifs, path)) {
        try {
            readLines(ifs, config);
        }
        catch (const ParseError & ex) {
            std::cerr << path << ":" << ex.message << std::endl;
        }
        return true;
    }
    else {
        return false;
    }
}

} // namespace {anonymous}

void parseConfig(Config & config) {
    const std::string conf = "/terminol/config";

    auto xdg_config_home = static_cast<const char *>(::getenv("XDG_CONFIG_HOME"));

    if (xdg_config_home) {
        if (tryConfig(xdg_config_home + conf, config)) {
            return;
        }
    }

    auto xdg_config_dirs = static_cast<const char *>(::getenv("XDG_CONFIG_DIRS"));

    if (xdg_config_dirs) {
        std::string str = xdg_config_dirs;
        size_t      i   = 0;

        for (;;) {
            size_t j = str.find(':', i);

            if (j == std::string::npos) { j = str.size(); }

            if (tryConfig(str.substr(i, j - i) + conf, config)) {
                return;
            }

            if (j == str.size()) { break; }
            else { i = j + 1; }
        }
    }

    auto home = static_cast<const char *>(::getenv("HOME"));

    if (home) {
        if (tryConfig(home + std::string("/.config") + conf, config)) {
            return;
        }
    }

    std::cerr << "No configuration file found." << std::endl;
}
