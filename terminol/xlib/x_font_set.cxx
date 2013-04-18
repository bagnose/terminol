// vi:noai:sw=4

#include "terminol/xlib/x_font_set.hxx"

X_FontSet::X_FontSet(Display           * display,
                     const std::string & fontName) throw (Error) :
    _display(display),
    _normal(nullptr),
    _bold(nullptr),
    _italic(nullptr),
    _italicBold(nullptr),
    _width(0),
    _height(0),
    _ascent(0)
{
    FcPattern * pattern;

    pattern = FcNameParse(reinterpret_cast<const FcChar8 *>(fontName.c_str()));
    if (!pattern) {
        throw Error("Failed to parse font name: " + fontName);
    }

    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    // Normal
    _normal = load(pattern, true);

    // Bold
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    _bold = load(pattern, false);

    // Italic
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternDel(pattern, FC_SLANT);
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    _italic = load(pattern, false);

    // Italic bold
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    _italicBold = load(pattern, false);

    FcPatternDestroy(pattern);
}

X_FontSet::~X_FontSet() {
    unload(_italicBold);
    unload(_italic);
    unload(_bold);
    unload(_normal);
}

XftFont * X_FontSet::load(FcPattern * pattern, bool master) throw (Error) {
    FcResult result;
    FcPattern * match = FcFontMatch(nullptr, pattern, &result);
    ENFORCE(match, "");

    // Note, the match will be free'd when the font is closed.
    XftFont * font = XftFontOpenPattern(_display, match);
    if (!font) {
        throw Error("Failed to load font.");
    }

    _width  = std::max(_width,  static_cast<uint16_t>(font->max_advance_width));
    _height = std::max(_height, static_cast<uint16_t>(font->height));

    if (master) {
        _ascent = font->ascent;
    }

    return font;
}

void X_FontSet::unload(XftFont * font) {
    XftFontClose(_display, font);
}
