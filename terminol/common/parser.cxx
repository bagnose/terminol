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

#if 0
std::string lowercase(const std::string & str) {
    auto copy = str;
    for (auto & c : copy) { c = tolower(c); }
    return copy;
}
#endif

bool lookupModifier(const std::string & name, Modifier & modifier) {
    if (name == "shift") {
        modifier = Modifier::SHIFT;
        return true;
    }
    else if (name == "alt") {
        modifier = Modifier::ALT;
        return true;
    }
    else if (name == "ctrl") {
        modifier = Modifier::CONTROL;
        return true;
    }
    else if (name == "super") {
        modifier = Modifier::SUPER;
        return true;
    }
    else if (name == "num_lock" || name == "num-lock") {
        modifier = Modifier::NUM_LOCK;
        return true;
    }
    else if (name == "shift_lock" || name == "shift-lock") {
        modifier = Modifier::SHIFT_LOCK;
        return true;
    }
    else if (name == "caps_lock" || name == "caps-lock") {
        modifier = Modifier::CAPS_LOCK;
        return true;
    }
    else if (name == "mode_switch" || name == "mode-switch") {
        modifier = Modifier::MODE_SWITCH;
        return true;
    }
    else {
        return false;
    }
}

void handleSet(const std::string & key,
               const std::string & value,
               Config & config) throw (ParseError) {
    if (key == "resizeStrategy") {
        config.resizeStrategy = unstringify<Resize>(value);
    }
    else if (key == "fontName") {
        config.fontName = value;
    }
    else if (key == "fontSize") {
        config.fontSize = unstringify<int>(value);
    }
    else if (key == "termName") {
        config.termName = value;
    }
    else if (key == "scrollWithHistory") {
        config.scrollWithHistory = unstringify<bool>(value);
    }
    else if (key == "scrollOnTtyOutput") {
        config.scrollOnTtyOutput = unstringify<bool>(value);
    }
    else if (key == "scrollOnTtyKeyPress") {
        config.scrollOnTtyKeyPress = unstringify<bool>(value);
    }
    else if (key == "scrollOnResize") {
        config.scrollOnResize = unstringify<bool>(value);
    }
    else if (key == "scrollOnPaste") {
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
    else if (key == "scrollBackHistory") {
        config.scrollBackHistory = unstringify<size_t>(value);
    }
    else if (key == "unlimitedScrollBack") {
        config.unlimitedScrollBack = unstringify<bool>(value);
    }
    else if (key == "reflowHistory") {
        config.reflowHistory = unstringify<size_t>(value);
    }
    else if (key == "framesPerSecond") {
        config.framesPerSecond = unstringify<int>(value);
    }
    else if (key == "traditionalWrapping") {
        config.traditionalWrapping = unstringify<bool>(value);
    }
    else if (key == "traceTty") {
        config.traceTty = unstringify<bool>(value);
    }
    else if (key == "syncTty") {
        config.syncTty = unstringify<bool>(value);
    }
    // initialX/Y/Rows/Columns
    else if (key == "normalFgColor") {
        config.normalFgColor = Color::fromString(value);
    }
    else if (key == "normalBgColor") {
        config.normalBgColor = Color::fromString(value);
    }
    // systemColors[16]
    else if (key == "cursorFillColor") {
        config.customCursorFillColor = true;
        config.cursorFillColor = Color::fromString(value);
    }
    else if (key == "cursorTextColor") {
        config.customCursorTextColor = true;
        config.cursorTextColor = Color::fromString(value);
    }
    else if (key == "scrollbarFgColor") {
        config.scrollbarFgColor = Color::fromString(value);
    }
    else if (key == "scrollbarBgColor") {
        config.scrollbarBgColor = Color::fromString(value);
    }
    else if (key == "scrollbarWidth") {
        config.scrollbarWidth = unstringify<int>(value);
    }
    else if (key == "borderColor") {
        config.borderColor = Color::fromString(value);
    }
    else if (key == "borderThickness") {
        config.borderThickness = unstringify<int>(value);
    }
    else if (key == "doubleClickTimeout") {
        config.doubleClickTimeout = unstringify<uint32_t>(value);
    }
    else if (key == "colorScheme") {
        config.setColorScheme(value);
    }
    else if (key == "serverFork") {
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
    else if (str == "scroll-up-one-line") {
        return Action::SCROLL_UP_ONE_LINE;
    }
    else if (str == "scroll-down-one-line") {
        return Action::SCROLL_DOWN_ONE_LINE;
    }
    else if (str == "scroll-up-one-page") {
        return Action::SCROLL_UP_ONE_PAGE;
    }
    else if (str == "scroll-down-one-page") {
        return Action::SCROLL_DOWN_ONE_PAGE;
    }
    else if (str == "scroll-top") {
        return Action::SCROLL_TOP;
    }
    else if (str == "scroll-bottom") {
        return Action::SCROLL_BOTTOM;
    }
    else if (str == "debug-modes") {
        return Action::DEBUG_MODES;
    }
    else if (str == "debug-stats") {
        return Action::DEBUG_STATS;
    }
    else if (str == "debug-buffer") {
        return Action::DEBUG_BUFFER;
    }
    else if (str == "debug-selection") {
        return Action::DEBUG_SELECTION;
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
            Modifier modifier;
            if (lookupModifier(m, modifier)) {
                //PRINT("Got modifier: " << modifier);
                modifiers.set(modifier);
            }
            else {
                throw ParseError("Bad modifier: '" + m + "'");
            }
        }

        auto keySym = xkb::nameToSym(key);

        KeyCombo keyCombo(keySym, modifiers);
        Action   action2 = lookupAction(action);

        PRINT("Bound: " << modifiers << "-" << xkb::symToName(keySym) << " to " << action);

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
            std::cerr << path << ":" << ex.message;
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

    const char * xdg_config_home = ::getenv("XDG_CONFIG_HOME");

    if (xdg_config_home) {
        if (tryConfig(xdg_config_home + conf, config)) {
            return;
        }
    }

    const char * xdg_config_dirs = ::getenv("XDG_CONFIG_DIRS");

    if (xdg_config_dirs) {
        std::string str = xdg_config_dirs;
        size_t i = 0;

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

    const char * home = ::getenv("HOME");

    if (home) {
        if (tryConfig(home + std::string("/.config") + conf, config)) {
            return;
        }
    }

    std::cerr << "No configuration file found." << std::endl;
}
