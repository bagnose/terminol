// vi:noai:sw=4

#include "terminol/common/config.hxx"
#include "terminol/support/conv.hxx"

#include <limits>

const uint8_t COLOURS_LINUX[16][3] = {
  { 0x00, 0x00, 0x00 },
  { 0xa8, 0x00, 0x00 },
  { 0x00, 0xa8, 0x00 },
  { 0xa8, 0x57, 0x00 },
  { 0x00, 0x00, 0xa8 },
  { 0xa8, 0x00, 0xa8 },
  { 0x00, 0xa8, 0xa8 },
  { 0xa8, 0xa8, 0xa8 },
  { 0x57, 0x57, 0x57 },
  { 0xff, 0x57, 0x57 },
  { 0x57, 0xff, 0x57 },
  { 0xff, 0xff, 0x57 },
  { 0x57, 0x57, 0xff },
  { 0xff, 0x57, 0xff },
  { 0x57, 0xff, 0xff },
  { 0xff, 0xff, 0xff }
};

const uint8_t COLOURS_RXVT[16][3] = {
  { 0x00, 0x00, 0x00 },
  { 0xcd, 0x00, 0x00 },
  { 0x00, 0xcd, 0x00 },
  { 0xcd, 0xcd, 0x00 },
  { 0x00, 0x00, 0xcd },
  { 0xcd, 0x00, 0xcd },
  { 0x00, 0xcd, 0xcd },
  { 0xfa, 0xeb, 0xd7 },
  { 0x40, 0x40, 0x40 },
  { 0xff, 0x00, 0x00 },
  { 0x00, 0xff, 0x00 },
  { 0xff, 0xff, 0x00 },
  { 0x00, 0x00, 0xff },
  { 0xff, 0x00, 0xff },
  { 0x00, 0xff, 0xff },
  { 0xff, 0xff, 0xff }
};

const uint8_t COLOURS_TANGO[16][3] = {
  { 0x2e, 0x34, 0x36 },
  { 0xcc, 0x00, 0x00 },
  { 0x4e, 0x9a, 0x06 },
  { 0xc4, 0xa0, 0x00 },
  { 0x34, 0x65, 0xa4 },
  { 0x75, 0x50, 0x7b },
  { 0x06, 0x98, 0x9a },
  { 0xd3, 0xd7, 0xcf },
  { 0x55, 0x57, 0x53 },
  { 0xef, 0x29, 0x29 },
  { 0x8a, 0xe2, 0x34 },
  { 0xfc, 0xe9, 0x4f },
  { 0x72, 0x9f, 0xcf },
  { 0xad, 0x7f, 0xa8 },
  { 0x34, 0xe2, 0xe2 },
  { 0xee, 0xee, 0xec }
};

const uint8_t COLOURS_XTERM[16][3] = {
  { 0x00, 0x00, 0x00 },
  { 0xcd, 0x00, 0x00 },
  { 0x00, 0xcd, 0x00 },
  { 0xcd, 0xcd, 0x00 },
  { 0x00, 0x00, 0xee },
  { 0xcd, 0x00, 0xcd },
  { 0x00, 0xcd, 0xcd },
  { 0xe5, 0xe5, 0xe5 },
  { 0x7f, 0x7f, 0x7f },
  { 0xff, 0x00, 0x00 },
  { 0x00, 0xff, 0x00 },
  { 0xff, 0xff, 0x00 },
  { 0x5c, 0x5c, 0xff },
  { 0xff, 0x00, 0xff },
  { 0x00, 0xff, 0xff },
  { 0xff, 0xff, 0xff }
};

const uint8_t COLOURS_ZENBURN_DARK[16][3] = {
  { 0x00, 0x00, 0x00 },
  { 0x9e, 0x18, 0x28 },
  { 0xae, 0xce, 0x92 },
  { 0x96, 0x8a, 0x38 },
  { 0x41, 0x41, 0x71 },
  { 0x96, 0x3c, 0x59 },
  { 0x41, 0x81, 0x79 },
  { 0xbe, 0xbe, 0xbe },
  { 0x66, 0x66, 0x66 },
  { 0xcf, 0x61, 0x71 },
  { 0xc5, 0xf7, 0x79 },
  { 0xff, 0xf7, 0x96 },
  { 0x41, 0x86, 0xbe },
  { 0xcf, 0x9e, 0xbe },
  { 0x71, 0xbe, 0xbe },
  { 0xff, 0xff, 0xff }
};

const uint8_t COLOURS_ZENBURN[16][3] = {
  { 0x3f, 0x3f, 0x3f },
  { 0x70, 0x50, 0x50 },
  { 0x60, 0xb4, 0x8a },
  { 0xdf, 0xaf, 0x8f },
  { 0x50, 0x60, 0x70 },
  { 0xdc, 0x8c, 0xc3 },
  { 0x8c, 0xd0, 0xd3 },
  { 0xdc, 0xdc, 0xcc },
  { 0x70, 0x90, 0x80 },
  { 0xdc, 0xa3, 0xa3 },
  { 0xc3, 0xbf, 0x9f },
  { 0xf0, 0xdf, 0xaf },
  { 0x94, 0xbf, 0xf3 },
  { 0xec, 0x93, 0xd3 },
  { 0x93, 0xe0, 0xe3 },
  { 0xff, 0xff, 0xff }
};

const uint8_t COLOURS_SOLARIZED_DARK[16][3] = {
  { 0x07, 0x36, 0x42 },
  { 0xdc, 0x32, 0x2f },
  { 0x85, 0x99, 0x00 },
  { 0xb5, 0x89, 0x00 },
  { 0x26, 0x8b, 0xd2 },
  { 0xd3, 0x36, 0x82 },
  { 0x2a, 0xa1, 0x98 },
  { 0xee, 0xe8, 0xd5 },
  { 0x00, 0x2b, 0x36 },
  { 0xcb, 0x4b, 0x16 },
  { 0x58, 0x6e, 0x75 },
  { 0x65, 0x7b, 0x83 },
  { 0x83, 0x94, 0x96 },
  { 0x6c, 0x71, 0xc4 },
  { 0x93, 0xa1, 0xa1 },
  { 0xfd, 0xf6, 0xe3 }
};

const uint8_t COLOURS_SOLARIZED_LIGHT[16][3] = {
  { 0xee, 0xe8, 0xd5 },
  { 0xdc, 0x32, 0x2f },
  { 0x85, 0x99, 0x00 },
  { 0xb5, 0x89, 0x00 },
  { 0x26, 0x8b, 0xd2 },
  { 0xd3, 0x36, 0x82 },
  { 0x2a, 0xa1, 0x98 },
  { 0x07, 0x36, 0x42 },
  { 0xfd, 0xf6, 0xe3 },
  { 0xcb, 0x4b, 0x16 },
  { 0x93, 0xa1, 0xa1 },
  { 0x83, 0x94, 0x96 },
  { 0x65, 0x7b, 0x83 },
  { 0x6c, 0x71, 0xc4 },
  { 0x58, 0x6e, 0x75 },
  { 0x00, 0x2b, 0x36 }
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
    _scrollBackHistory(4096),
    _unlimitedScrollBack(false),
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
    switch (6) {
        case 0:     // LINUX
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_LINUX[i]);
            }

            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            _customCursorFillColor = false;
            break;
        case 1:     // RXVT
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_RXVT[i]);
            }

            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            _customCursorFillColor = false;
            break;
        case 2:     // TANGO
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_TANGO[i]);
            }

            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            _customCursorFillColor = false;
            break;
        case 3:     // XTERM
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_XTERM[i]);
            }

            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            _customCursorFillColor = false;
            break;
        case 4:     // ZENBURN DARK
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_ZENBURN_DARK[i]);
            }

            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            _customCursorFillColor = false;
            break;
        case 5:     // ZENBURN
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_ZENBURN[i]);
            }

            _fgColor = _systemColors[7];
            _bgColor = _systemColors[0];
            _customCursorFillColor = false;
            break;
        case 6:     // SOLARIZED DARK
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_SOLARIZED_DARK[i]);
            }
            _fgColor = _systemColors[12];
            _bgColor = _systemColors[8];
            _customCursorFillColor = true;
            _cursorFillColor       = _systemColors[14];
            break;
        case 7:     // SOLARIZED LIGHT
            for (size_t i = 0; i != 16; ++i) {
                _systemColors[i] = decodeTripletColor(COLOURS_SOLARIZED_LIGHT[i]);
            }
            _fgColor = _systemColors[12];
            _bgColor = _systemColors[8];
            _customCursorFillColor = true;
            _cursorFillColor       = _systemColors[14];
            break;
    }

    _customCursorFillColor = false;

    _scrollbarFgColor = { 0.5, 0.5, 0.5 };
    _scrollbarBgColor = _bgColor;
    _borderColor      = _bgColor;

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

Config::Color Config::decodeTripletColor(const uint8_t values[3]) {
    const double MAX = std::numeric_limits<uint8_t>::max();

    Color color = {
        static_cast<double>(values[0]) / MAX,
        static_cast<double>(values[1]) / MAX,
        static_cast<double>(values[2]) / MAX
    };

    return color;
}
