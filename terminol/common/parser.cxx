// vi:noai:sw=4

#include "terminol/common/parser.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"

#include <fstream>

#include <xkbcommon/xkbcommon.h>

bool split(const std::string        & line,
           std::vector<std::string> & tokens) throw (ParseError) {
    auto i = line.find_first_not_of(" \t");

    if (i == std::string::npos) { return false; }   // blank line
    if (line[i] == '#')         { return false; }   // comment

    for (;;) {
        bool quote = line[i] == '"';

        auto j = line.find_first_of(quote ? "\"" : " \t", i + 1);
        if (quote) { ++i; }

        if (j == std::string::npos) {
            if (quote) {
                throw ParseError("Unterminated quote.");
            }
            else {
                j = line.size();
            }
        }

        tokens.push_back(line.substr(i, j - i));

        if (j == line.size()) { break; }

        i = line.find_first_not_of(" \t", j + 1);

        if (i == std::string::npos) { break; }
    }

    return true;
}

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
    else if (name == "num_lock") {      // XXX ??
        modifier = Modifier::NUM_LOCK;
        return true;
    }
    else if (name == "shift_lock") {      // XXX ??
        modifier = Modifier::SHIFT_LOCK;
        return true;
    }
    else if (name == "caps_lock") {      // XXX ??
        modifier = Modifier::CAPS_LOCK;
        return true;
    }
    else if (name == "mode_switch") {      // XXX ??
        modifier = Modifier::MODE_SWITCH;
        return true;
    }
    else {
        return false;
    }
}

bool lookupKeySym(const std::string & name, xkb_keysym_t & keySym) {
    keySym = xkb_keysym_from_name(name.c_str(), XKB_KEYSYM_CASE_INSENSITIVE);
    return keySym != XKB_KEY_NoSymbol;
}
#endif

//bool lookupButton(const std::string & name, 

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
        ERROR("Unknown setting '" << key << "'");
    }
}

void handleBindSym(const std::vector<std::string> & UNUSED(tokens),
                   Config & UNUSED(config)) {
    /*
    xkb_keysym_t keySym;
    if (lookupKeySym(t, keySym)) {
        PRINT(t << " == " << keySym);
    }
    else {
        PRINT("Failed to resolve: " << t);
    }
    */
}

void interpretTokens(const std::vector<std::string> & tokens,
                     Config & config) throw (ParseError) {
    ASSERT(!tokens.empty(), "No tokens!");

    if (tokens[0] == "set") {
        if (tokens.size() == 3) {
            handleSet(tokens[1], tokens[2], config);
        }
        else {
            // ERROR
            ERROR("Wrong number of tokens: " << tokens.size());
        }
    }
    else if (tokens[0] == "bindSym") {
        handleBindSym(tokens, config);
    }
    else {
        // ERROR
        ERROR("Unrecognised starting token: " << tokens[0]);
    }
}

void readLines(std::istream & ist, Config & config) {
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
            ERROR("Line: " << num << ": " << ex.message);
        }
    }
}

} // namespace {anonymous}

void parseConfig(Config & config) {
    const std::string conf = "/terminol/config";

    std::ifstream ifs;

    const char * xdg_config_home = ::getenv("XDG_CONFIG_HOME");

    if (xdg_config_home) {
        if (open(ifs, xdg_config_home + conf)) {
            readLines(ifs, config);
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

            if (open(ifs, str.substr(i, j - i) + conf)) {
                readLines(ifs, config);
                return;
            }

            if (j == str.size()) { break; }
            else { i = j + 1; }
        }
    }

    const char * home = ::getenv("HOME");

    if (home) {
        if (open(ifs, home + std::string("/.config") + conf)) {
            readLines(ifs, config);
            return;
        }
    }
}
