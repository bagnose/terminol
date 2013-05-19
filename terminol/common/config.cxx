// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/support/conv.hxx"

#include <limits>

namespace {

const char * S_yellow  = "#b58900";
const char * S_orange  = "#cb4b16";
const char * S_red     = "#dc322f";
const char * S_magenta = "#d33682";
const char * S_violet  = "#6c71c4";
const char * S_blue    = "#268bd2";
const char * S_cyan    = "#2aa198";
const char * S_green   = "#859900";


namespace dark {

const char * S_base03  = "#002b36";
const char * S_base02  = "#073642";
const char * S_base01  = "#586e75";
const char * S_base00  = "#657b83";
const char * S_base0   = "#839496";
const char * S_base1   = "#93a1a1";
const char * S_base2   = "#eee8d5";
const char * S_base3   = "#fdf6e3";

} // namespace dark

namespace light {

const char * S_base03  = "#fdf6e3";
const char * S_base02  = "#eee8d5";
const char * S_base01  = "#93a1a1";
const char * S_base00  = "#839496";
const char * S_base0   = "#657b83";
const char * S_base1   = "#586e75";
const char * S_base2   = "#073642";
const char * S_base3   = "#002b36";

} // namespace light

} // namespace {anonymous}

const char * Config::SOLARIZED_LIGHT_COLORS[16] = {
    light::S_base02,
    S_red,
    S_green,
    S_yellow,
    S_blue,
    S_magenta,
    S_cyan,
    light::S_base2,
    light::S_base03,
    S_orange,
    light::S_base01,
    light::S_base00,
    light::S_base0,
    S_violet,
    light::S_base1,
    light::S_base3
};

const char * Config::SOLARIZED_DARK_COLORS[16] = {
    dark::S_base02,
    S_red,
    S_green,
    S_yellow,
    S_blue,
    S_magenta,
    S_cyan,
    dark::S_base2,
    dark::S_base03,
    S_orange,
    dark::S_base01,
    dark::S_base00,
    dark::S_base0,
    S_violet,
    dark::S_base1,
    dark::S_base3
};

const Config::Color Config::STANDARD_COLORS[16] = {
    // normal
    { 0.0,  0.0,  0.0  }, // black
    { 0.66, 0.0,  0.0  }, // red
    { 0.0,  0.66, 0.0  }, // green
    { 0.66, 0.66, 0.0  }, // orange
    { 0.0,  0.0,  0.66 }, // blue
    { 0.66, 0.0,  0.66 }, // magenta
    { 0.0,  0.66, 0.66 }, // cyan
    { 0.66, 0.66, 0.66 }, // light grey
    // bright
    { 0.33, 0.33, 0.33 }, // dark grey
    { 1.0,  0.33, 0.33 }, // high red
    { 0.33, 1.0,  0.33 }, // high green
    { 1.0,  1.0,  0.33 }, // high yellow
    { 0.33, 0.33, 1.0  }, // high blue
    { 1.0,  0.33, 1.0  }, // high magenta
    { 0.33, 1.0,  1.0  }, // high cyan
    { 1.0,  1.0,  1.0  }  // white
};

const Config::Color Config::TWEAKED_COLORS[16] = {
    // normal
    { 0.0,  0.0,  0.08 }, // black
    { 0.66, 0.0,  0.0  }, // red
    { 0.0,  0.66, 0.0  }, // green
    { 0.66, 0.66, 0.0  }, // orange
    { 0.33, 0.33, 0.90 }, // blue
    { 0.66, 0.0,  0.66 }, // magenta
    { 0.0,  0.66, 0.66 }, // cyan
    { 0.66, 0.66, 0.66 }, // light grey
    // bright
    { 0.33, 0.33, 0.33 }, // dark grey
    { 1.0,  0.33, 0.33 }, // high red
    { 0.33, 1.0,  0.33 }, // high green
    { 1.0,  1.0,  0.33 }, // high yellow
    { 0.66, 0.66, 1.0  }, // high blue
    { 1.0,  0.33, 1.0  }, // high magenta
    { 0.33, 1.0,  1.0  }, // high cyan
    { 1.0,  1.0,  1.0  }  // white
};

//
//
//

Config::Config() :
    //_fontName("inconsolata:pixelsize=18"),
    _fontName("MesloLGM:pixelsize=15"),
    _geometryString(),
    _termName("xterm-256color"),
    _scrollWithHistory(false),
    _scrollOnTtyOutput(false),
    _scrollOnTtyKeyPress(true),
    _doubleBuffer(true),
    _title("terminol"),
    _chdir(),
    _scrollBackHistory(1024),
    _unlimitedScrollBack(true),
    //
    _traceTty(false),
    _syncTty(false),
    //
    _customCursorFillColor(false),
    _customCursorTextColor(false),
    //
    _scrollbarWidth(8),
    //
    _borderThickness(1)
{
    switch (0) {
        case 0:
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeHexColor(SOLARIZED_DARK_COLORS[i]);
            }
            _fgColor = _systemColors[12];
            _bgColor = _systemColors[8];

            _customCursorFillColor = true;
            _cursorFillColor       = _systemColors[14];

            _customCursorFillColor = false;

            _scrollbarFgColor = { 0.5, 0.5, 0.5 };
            _scrollbarBgColor = _bgColor;

            _borderColor      = _bgColor;
            break;
        case 1:
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeHexColor(SOLARIZED_LIGHT_COLORS[i]);
            }
            _fgColor = _systemColors[12];
            _bgColor = _systemColors[8];

            _customCursorFillColor = true;
            _cursorFillColor       = _systemColors[14];

            _customCursorFillColor = false;

            _scrollbarFgColor = { 0.5, 0.5, 0.5 };
            _scrollbarBgColor = _bgColor;

            _borderColor      = _bgColor;
            break;
        case 2:
            memcpy(_systemColors, STANDARD_COLORS, sizeof(STANDARD_COLORS));
            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            break;
        case 3:
            memcpy(_systemColors, TWEAKED_COLORS, sizeof(TWEAKED_COLORS));
            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            break;
    }

    std::ostringstream ost;
    ost << "/tmp/terminols-" << ::getenv("USER");
    _socketPath = ost.str();
}

Config::Color Config::decodeHexColor(const std::string & hexColor) {
    ASSERT(hexColor.size() == 7, "");
    ASSERT(hexColor.front() == '#', "");

    uint8_t r = hexToByte(hexColor[1], hexColor[2]);
    uint8_t g = hexToByte(hexColor[3], hexColor[4]);
    uint8_t b = hexToByte(hexColor[5], hexColor[6]);

    const double MAX = std::numeric_limits<uint8_t>::max();

    Color color = {
        static_cast<double>(r) / MAX,
        static_cast<double>(g) / MAX,
        static_cast<double>(b) / MAX
    };

    return color;
}
