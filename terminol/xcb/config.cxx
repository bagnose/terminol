// vi:noai:sw=4

#include "terminol/xcb/config.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/debug.hxx"
#include <fstream>

namespace {

bool open(std::ifstream & ifs, const std::string & path) {
    ifs.open(path.c_str());
    return ifs.good();
}

bool split(std::string & line, std::string & lhs, std::string & rhs) {
    auto lhs0 = line.find_first_not_of(" \t");

    if (lhs0 == std::string::npos) { return false; }
    if (line[lhs0] == '#') { return false; }
    auto lhs1 = line.find_first_of(" \t=", lhs0);

    if (lhs1 == std::string::npos) { return false; }
    auto equals = line.find_first_of("=", lhs1);

    if (equals == std::string::npos) { return false; }
    auto rhs0 = line.find_first_not_of(" \t", equals + 1);

    if (rhs0 == std::string::npos) { return false; }
    auto rhs1 = line.find_last_of(" \t", rhs0);

    if (rhs1 == std::string::npos) { rhs1 = line.size(); }

    lhs = line.substr(lhs0, lhs1 - lhs0);
    rhs = line.substr(rhs0, rhs1 - rhs0);

    return true;
}

void read(std::istream & ist, Config & config) {
    std::string line;
    while (getline(ist, line).good()) {
        std::string lhs, rhs;
        if (split(line, lhs, rhs)) {
            try {
                if (lhs == "scrollBackHistory") {
                    config.setScrollBackHistory(unstringify<size_t>(rhs));
                }
                else if (lhs == "serverFork") {
                    config.setServerFork(unstringify<bool>(rhs));
                }
                else if (lhs == "fontName") {
                    config.setFontName(rhs);
                }
                else if (lhs == "fontSize") {
                    config.setFontSize(unstringify<int>(rhs));
                }
                else {
                    ERROR("Unknown setting '" << lhs << "'");
                }
            }
            catch (...) {
                PRINT("Error handling: " << lhs << " " << rhs);
            }
        }
    }
}

} // namespace {anonymous}

void readConfig(Config & config) {
    const std::string conf = "/terminol/config";

    std::ifstream ifs;

    const char * xdg_config_home = ::getenv("XDG_CONFIG_HOME");

    if (xdg_config_home) {
        if (open(ifs, xdg_config_home + conf)) {
            read(ifs, config);
            return;
        }
    }

    const char * xdg_config_dirs = ::getenv("XDG_CONFIG_DIRS");

    if (xdg_config_dirs) {
        std::string str = xdg_config_dirs;
        size_t i = 0;

        for (;;) {
            size_t j = str.find(':', i);

            if (open(ifs, str.substr(i, j - i) + conf)) {
                read(ifs, config);
                return;
            }

            if (j == std::string::npos) {
                break;
            }
            else {
                i = j + 1;
            }
        }
    }

    const char * home = ::getenv("HOME");

    if (home) {
        if (open(ifs, home + std::string("/.config") + conf)) {
            read(ifs, config);
            return;
        }
    }
}
