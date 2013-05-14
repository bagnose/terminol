// vi:noai:sw=4

#include "terminol/common/config.hxx"

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

/*
URxvt*background:              S_base03
URxvt*foreground:              S_base0
URxvt*fading:                  40
URxvt*fadeColor:               S_base03
URxvt*cursorColor:             S_base1
URxvt*pointerColorBackground:  S_base01
URxvt*pointerColorForeground:  S_base1
*/

/*
URxvt*color0:                  S_base02
URxvt*color1:                  S_red
URxvt*color2:                  S_green
URxvt*color3:                  S_yellow
URxvt*color4:                  S_blue
URxvt*color5:                  S_magenta
URxvt*color6:                  S_cyan
URxvt*color7:                  S_base2
URxvt*color9:                  S_orange
URxvt*color8:                  S_base03
URxvt*color10:                 S_base01
URxvt*color11:                 S_base00
URxvt*color12:                 S_base0
URxvt*color13:                 S_violet
URxvt*color14:                 S_base1
URxvt*color15:                 S_base3
*/

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
    _scrollOnTtyOutput(false),
    _scrollOnTtyKeyPress(true),
    _doubleBuffer(false),
    _title(),
    _chdir(),
    _scrollBackHistory(100),
    _traceTty(false),
    _syncTty(false),
    _systemColors()
{
    switch (0) {
        case 0:
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeHexColor(SOLARIZED_DARK_COLORS[i]);
            }
            break;
        case 1:
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeHexColor(SOLARIZED_LIGHT_COLORS[i]);
            }
            break;
        case 2:
            memcpy(_systemColors, STANDARD_COLORS, sizeof(STANDARD_COLORS));
            break;
        case 3:
            PRINT(sizeof(STANDARD_COLORS));
            memcpy(_systemColors, TWEAKED_COLORS, sizeof(STANDARD_COLORS));
            break;
    }
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
