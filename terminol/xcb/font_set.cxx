// vi:noai:sw=4

#include "terminol/xcb/font_set.hxx"

FontSet::FontSet(const Config & config) throw (Error) :
    _config(config),
    _firstLoad(true),
    _normal(nullptr),
    _bold(nullptr),
    _italic(nullptr),
    _italicBold(nullptr),
    _width(0),
    _height(0),
    _ascent(0)
{
    FcPattern * pattern;

    const std::string & fontName = _config.getFontName();

    pattern = FcNameParse(reinterpret_cast<const FcChar8 *>(fontName.c_str()));
    if (!pattern) {
        throw Error("Failed to parse font name: " + fontName);
    }

    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    // Normal
    _normal = load(pattern);

    // Bold
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    _bold = load(pattern);

    // Italic
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternDel(pattern, FC_SLANT);
    FcPatternAddInteger(pattern, FC_SLANT, FC_SLANT_ITALIC);
    _italic = load(pattern);

    // Italic bold
    FcPatternDel(pattern, FC_WEIGHT);
    FcPatternAddInteger(pattern, FC_WEIGHT, FC_WEIGHT_BOLD);
    _italicBold = load(pattern);

    FcPatternDestroy(pattern);
}

FontSet::~FontSet() {
    unload(_italicBold);
    unload(_italic);
    unload(_bold);
    unload(_normal);
}

cairo_scaled_font_t * FontSet::load(FcPattern * pattern) throw (Error) {
    FcResult result;
    FcPattern * match = FcFontMatch(nullptr, pattern, &result);
    ENFORCE(match, "");

    // Disable anti-aliasing
    //FcPatternDel(match, FC_ANTIALIAS);
    //FcPatternAddBool(match, FC_ANTIALIAS, FcFalse);

    // Disable auto-hinting
    //FcPatternDel(match, FC_AUTOHINT);
    //FcPatternAddBool(match, FC_AUTOHINT, FcFalse);

    double pixelSize;
    FcPatternGetDouble(match, FC_PIXEL_SIZE, 0, &pixelSize);
    //PRINT("pixelsize: " << pixelSize);

    cairo_font_face_t * fontFace = cairo_ft_font_face_create_for_pattern(match);
    FcPatternDestroy(match);

    if (!fontFace) {
        throw Error("Failed to load font.");
    }

    cairo_matrix_t fontMatrix, ctm;
    cairo_matrix_init_identity(&fontMatrix);
    cairo_matrix_scale(&fontMatrix, pixelSize, pixelSize);
    cairo_matrix_init_identity(&ctm);

    cairo_font_options_t * fontOptions = cairo_font_options_create();
    cairo_scaled_font_t  * scaledFont  = cairo_scaled_font_create(fontFace,
                                                                  &fontMatrix,
                                                                  &ctm,
                                                                  fontOptions);
    cairo_font_options_destroy(fontOptions);

    /*
       typedef struct {
       double ascent;
       double descent;
       double height;
       double max_x_advance;
       double max_y_advance;
       } cairo_font_extents_t;
       */

    cairo_font_extents_t extents;
    cairo_scaled_font_extents(scaledFont, &extents);

    /*
       PRINT("ascent=" << extents.ascent << ", descent=" << extents.descent <<
       ", height=" << extents.height << ", max_x_advance=" << extents.max_x_advance);
       */

    if (_firstLoad) {
        _width  = extents.max_x_advance;
        _height = extents.height;
        _ascent = extents.ascent;
        _firstLoad = false;
    }
    else {
        // FIXME this is way too tough a strategy.
        ASSERT(_width  == extents.max_x_advance, "");
        ASSERT(_height == extents.height, "");
        ASSERT(_ascent == extents.ascent, "");
    }

    return scaledFont;
}

void FontSet::unload(cairo_scaled_font_t * scaledFont) {
    cairo_font_face_t * fontFace = cairo_scaled_font_get_font_face(scaledFont);
    cairo_scaled_font_destroy(scaledFont);
    cairo_font_face_destroy(fontFace);
}
