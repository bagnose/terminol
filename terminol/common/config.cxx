// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/config.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/exception.hxx"

#include <limits>

namespace {

    constexpr std::array<Color, 16> COLOURS_LINUX = {{{0x00, 0x00, 0x00},
                                                      {0xA8, 0x00, 0x00},
                                                      {0x00, 0xA8, 0x00},
                                                      {0xA8, 0x57, 0x00},
                                                      {0x00, 0x00, 0xA8},
                                                      {0xA8, 0x00, 0xA8},
                                                      {0x00, 0xA8, 0xA8},
                                                      {0xA8, 0xA8, 0xA8},
                                                      {0x57, 0x57, 0x57},
                                                      {0xFF, 0x57, 0x57},
                                                      {0x57, 0xFF, 0x57},
                                                      {0xFF, 0xFF, 0x57},
                                                      {0x57, 0x57, 0xFF},
                                                      {0xFF, 0x57, 0xFF},
                                                      {0x57, 0xFF, 0xFF},
                                                      {0xFF, 0xFF, 0xFF}}};

    constexpr std::array<Color, 16> COLOURS_RXVT = {{{0x00, 0x00, 0x00},
                                                     {0xCD, 0x00, 0x00},
                                                     {0x00, 0xCD, 0x00},
                                                     {0xCD, 0xCD, 0x00},
                                                     {0x00, 0x00, 0xCD},
                                                     {0xCD, 0x00, 0xCD},
                                                     {0x00, 0xCD, 0xCD},
                                                     {0xFA, 0xEB, 0xD7},
                                                     {0x40, 0x40, 0x40},
                                                     {0xFF, 0x00, 0x00},
                                                     {0x00, 0xFF, 0x00},
                                                     {0xFF, 0xFF, 0x00},
                                                     {0x00, 0x00, 0xFF},
                                                     {0xFF, 0x00, 0xFF},
                                                     {0x00, 0xFF, 0xFF},
                                                     {0xFF, 0xFF, 0xFF}}};

    constexpr std::array<Color, 16> COLOURS_TANGO = {{{0x2E, 0x34, 0x36},
                                                      {0xCC, 0x00, 0x00},
                                                      {0x4E, 0x9A, 0x06},
                                                      {0xC4, 0xA0, 0x00},
                                                      {0x34, 0x65, 0xA4},
                                                      {0x75, 0x50, 0x7B},
                                                      {0x06, 0x98, 0x9A},
                                                      {0xD3, 0xD7, 0xCF},
                                                      {0x55, 0x57, 0x53},
                                                      {0xEF, 0x29, 0x29},
                                                      {0x8A, 0xE2, 0x34},
                                                      {0xFC, 0xE9, 0x4F},
                                                      {0x72, 0x9F, 0xCF},
                                                      {0xAD, 0x7F, 0xA8},
                                                      {0x34, 0xE2, 0xE2},
                                                      {0xEE, 0xEE, 0xEC}}};

    constexpr std::array<Color, 16> COLOURS_XTERM = {{{0x00, 0x00, 0x00},
                                                      {0xCD, 0x00, 0x00},
                                                      {0x00, 0xCD, 0x00},
                                                      {0xCD, 0xCD, 0x00},
                                                      {0x00, 0x00, 0xEE},
                                                      {0xCD, 0x00, 0xCD},
                                                      {0x00, 0xCD, 0xCD},
                                                      {0xE5, 0xE5, 0xE5},
                                                      {0x7F, 0x7F, 0x7F},
                                                      {0xFF, 0x00, 0x00},
                                                      {0x00, 0xFF, 0x00},
                                                      {0xFF, 0xFF, 0x00},
                                                      {0x5C, 0x5C, 0xFF},
                                                      {0xFF, 0x00, 0xFF},
                                                      {0x00, 0xFF, 0xFF},
                                                      {0xFF, 0xFF, 0xFF}}};

    constexpr std::array<Color, 16> COLOURS_ZENBURN_DARK = {{{0x00, 0x00, 0x00},
                                                             {0x9E, 0x18, 0x28},
                                                             {0xAE, 0xCE, 0x92},
                                                             {0x96, 0x8A, 0x38},
                                                             {0x41, 0x41, 0x71},
                                                             {0x96, 0x3C, 0x59},
                                                             {0x41, 0x81, 0x79},
                                                             {0xBE, 0xBE, 0xBE},
                                                             {0x66, 0x66, 0x66},
                                                             {0xCF, 0x61, 0x71},
                                                             {0xC5, 0xF7, 0x79},
                                                             {0xFF, 0xF7, 0x96},
                                                             {0x41, 0x86, 0xBE},
                                                             {0xCF, 0x9E, 0xBE},
                                                             {0x71, 0xBE, 0xBE},
                                                             {0xFF, 0xFF, 0xFF}}};

    constexpr std::array<Color, 16> COLOURS_ZENBURN = {{{0x3F, 0x3F, 0x3F},
                                                        {0x70, 0x50, 0x50},
                                                        {0x60, 0xB4, 0x8A},
                                                        {0xDF, 0xAF, 0x8F},
                                                        {0x50, 0x60, 0x70},
                                                        {0xDC, 0x8C, 0xC3},
                                                        {0x8C, 0xD0, 0xD3},
                                                        {0xDC, 0xDC, 0xCC},
                                                        {0x70, 0x90, 0x80},
                                                        {0xDC, 0xA3, 0xA3},
                                                        {0xC3, 0xBF, 0x9F},
                                                        {0xF0, 0xDF, 0xAF},
                                                        {0x94, 0xBF, 0xF3},
                                                        {0xEC, 0x93, 0xD3},
                                                        {0x93, 0xE0, 0xE3},
                                                        {0xFF, 0xFF, 0xFF}}};

    constexpr std::array<Color, 16> COLOURS_SOLARIZED_DARK = {{
        {0x07, 0x36, 0x42}, //  0 base02
        {0xDC, 0x32, 0x2F}, //  1 red
        {0x85, 0x99, 0x00}, //  2 green
        {0xB5, 0x89, 0x00}, //  3 yellow
        {0x26, 0x8B, 0xD2}, //  4 blue
        {0xD3, 0x36, 0x82}, //  5 magenta
        {0x2A, 0xA1, 0x98}, //  6 cyan
        {0xEE, 0xE8, 0xD5}, //  7 base2
        {0x00, 0x2B, 0x36}, //  8 base03
        {0xCB, 0x4B, 0x16}, //  9 orange
        {0x58, 0x6E, 0x75}, // 10 base01
        {0x65, 0x7B, 0x83}, // 11 base00
        {0x83, 0x94, 0x96}, // 12 base0
        {0x6C, 0x71, 0xC4}, // 13 violet
        {0x93, 0xA1, 0xA1}, // 14 base1
        {0xFD, 0xF6, 0xE3}  // 15 base3
    }};

    constexpr std::array<Color, 16> COLOURS_SOLARIZED_LIGHT = {{{0xEE, 0xE8, 0xD5},
                                                                {0xDC, 0x32, 0x2F},
                                                                {0x85, 0x99, 0x00},
                                                                {0xB5, 0x89, 0x00},
                                                                {0x26, 0x8B, 0xD2},
                                                                {0xD3, 0x36, 0x82},
                                                                {0x2A, 0xA1, 0x98},
                                                                {0x07, 0x36, 0x42},
                                                                {0xFD, 0xF6, 0xE3},
                                                                {0xCB, 0x4B, 0x16},
                                                                {0x93, 0xA1, 0xA1},
                                                                {0x83, 0x94, 0x96},
                                                                {0x65, 0x7B, 0x83},
                                                                {0x6C, 0x71, 0xC4},
                                                                {0x58, 0x6E, 0x75},
                                                                {0x00, 0x2B, 0x36}}};

} // namespace

//
//
//

Config::Config() {
    setColorScheme("rxvt");

    auto user = static_cast<const char *>(::getenv("USER"));
    if (!user) { user = "unknown-user"; }

    std::ostringstream ost;
    ost << "/tmp/terminols-" << user;
    socketPath = ost.str();
}

void Config::setColorScheme(const std::string & name) {
    if (name == "rxvt") {
        systemColors  = COLOURS_RXVT;
        normalFgColor = systemColors[7];
        normalBgColor = systemColors[0];
    }
    else if (name == "linux") {
        std::copy(COLOURS_LINUX.begin(), COLOURS_LINUX.end(), systemColors.begin());

        normalFgColor = systemColors[7];
        normalBgColor = systemColors[0];
    }
    else if (name == "tango") {
        systemColors  = COLOURS_TANGO;
        normalFgColor = systemColors[7];
        normalBgColor = systemColors[0];
    }
    else if (name == "xterm") {
        systemColors  = COLOURS_XTERM;
        normalFgColor = systemColors[7];
        normalBgColor = systemColors[0];
    }
    else if (name == "zenburn-dark") {
        systemColors  = COLOURS_ZENBURN_DARK;
        normalFgColor = systemColors[7];
        normalBgColor = systemColors[0];
    }
    else if (name == "zenburn") {
        systemColors  = COLOURS_ZENBURN;
        normalFgColor = systemColors[7];
        normalBgColor = systemColors[0];
    }
    else if (name == "solarized-dark") {
        systemColors  = COLOURS_SOLARIZED_DARK;
        normalFgColor = systemColors[12];
        normalBgColor = systemColors[8];
    }
    else if (name == "solarized-light") {
        systemColors  = COLOURS_SOLARIZED_LIGHT;
        normalFgColor = systemColors[12];
        normalBgColor = systemColors[8];
    }
    else {
        THROW(UserError("No such color scheme: " + name));
    }

    scrollbarFgColor = {0x7F, 0x7F, 0x7F};
    scrollbarBgColor = normalBgColor;
    borderColor      = normalBgColor;
    visualBellColor  = {0x7F, 0x7F, 0x7F};
}
