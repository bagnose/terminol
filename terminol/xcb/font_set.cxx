// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/xcb/font_set.hxx"
#include "terminol/support/pattern.hxx"

#include <cairo/cairo-xcb.h>
#include <pango/pangocairo.h>

FontSet::FontSet(const Config & config, Basics & basics, int size)
    : _config(config), _basics(basics) {
    ASSERT(size > 0, );
    auto & name = _config.fontName;

    _normal = load(name, size, true, false, false);
    ScopeGuard normalGuard([&]() { unload(_normal); });

    try {
        _bold = load(name, size, false, true, false);
    }
    catch (const Exception &) {
        std::cerr << "Using non-bold font" << std::endl;
        _bold = pango_font_description_copy(_normal);
    }
    ScopeGuard boldGuard([&]() { unload(_bold); });

    try {
        _italic = load(name, size, false, false, true);
    }
    catch (const Exception &) {
        std::cerr << "Using non-italic font" << std::endl;
        _italic = pango_font_description_copy(_normal);
    }
    ScopeGuard italicGuard([&]() { unload(_italic); });

    try {
        _italicBold = load(name, size, false, true, true);
    }
    catch (const Exception &) {
        std::cerr << "Note, trying non-bold, italic font" << std::endl;
        try {
            _italicBold = load(name, size, false, false, true);
        }
        catch (const Exception &) {
            std::cerr << "Using trying non-bold, non-italic font" << std::endl;
            _italicBold = pango_font_description_copy(_normal);
        }
    }
    ScopeGuard italicBoldGuard([&]() { unload(_italicBold); });

    // Dismiss guards
    italicBoldGuard.dismiss();
    italicGuard.dismiss();
    boldGuard.dismiss();
    normalGuard.dismiss();
}

FontSet::~FontSet() {
    unload(_italicBold);
    unload(_italic);
    unload(_bold);
    unload(_normal);
}

PangoFontDescription *
FontSet::load(const std::string & family, int size, bool master, bool bold, bool italic) {
#if 0
    {
        PangoFontMap * fontmap = pango_cairo_font_map_get_default();

        PangoFontFamily ** families;
        int n_families;
        pango_font_map_list_families(fontmap, &families, &n_families);

        //printf("There are %d families\n", n_families);
        for (int i = 0; i < n_families; i++) {
            PangoFontFamily * family2 = families[i];
            if (pango_font_family_is_monospace(family2)) {
                const char * family_name = pango_font_family_get_name(family2);
                printf ("Family %d: %s\n", i, family_name);
            }
        }
        g_free(families);
    }
#endif

    auto       desc = pango_font_description_new();
    ScopeGuard descGuard([&]() { pango_font_description_free(desc); });
    pango_font_description_set_family(desc, family.c_str());
    // pango_font_description_set_size(desc, size * PANGO_SCALE);
    pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
    pango_font_description_set_weight(desc, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_font_description_set_style(desc, italic ? PANGO_STYLE_OBLIQUE : PANGO_STYLE_NORMAL);

    /*
    auto str = pango_font_description_to_string(desc);
    PRINT("Got: " << str);
    std::free(str);
    */

    uint16_t width, height;
    measure(desc, width, height);

    if (master) {
        // The master font (non-italic, non-bold) sets the precedence
        // for the others to follow.
        _width  = width;
        _height = height;
    }
    else if (_width != width || _height != height) {
        // A non-master (subsequent) font doesn't conform with the
        // master font's metrics.
        THROW(GenericError(
            stringify("Size mismatch: ", (bold ? "bold" : ""), " ", (italic ? "italic" : ""))));
    }

    descGuard.dismiss();

    return desc;
}

void FontSet::unload(PangoFontDescription * desc) {
    pango_font_description_free(desc);
}

void FontSet::measure(PangoFontDescription * desc, uint16_t & width, uint16_t & height) {
    auto       surface = cairo_xcb_surface_create(_basics.connection(),
                                            _basics.screen()->root,
                                            _basics.visual(),
                                            1,
                                            1);
    ScopeGuard surfaceGuard([&]() { cairo_surface_destroy(surface); });

    auto       cr = cairo_create(surface);
    ScopeGuard crGuard([&]() { cairo_destroy(cr); });

    auto       layout = pango_cairo_create_layout(cr);
    ScopeGuard layoutGuard([&]() { g_object_unref(layout); });

    pango_layout_set_font_description(layout, desc);

    pango_layout_set_text(layout, "M", -1);
    pango_cairo_update_layout(cr, layout); // Required?

    PangoRectangle inkRect, logicalRect;
    pango_layout_get_extents(layout, &inkRect, &logicalRect);

#if 0
    double d = PANGO_SCALE;
    PRINT("inkRect: " << inkRect.x/d << " " << inkRect.y/d << " " << inkRect.width/d << " " << inkRect.height/d);
    PRINT("logicalRect: " << logicalRect.x/d << " " << logicalRect.y/d << " " << logicalRect.width/d << " " << logicalRect.height/d);
#endif

    /*
    PRINT(family << " " <<
          (bold ? "bold" : "normal") << " " <<
          (italic ? "italic" : "normal") << " " <<
          "WxH: " << width << "x" << height << std::endl);
          */

    if (int i = logicalRect.width % PANGO_SCALE) { WARNING("Width remainder: " << i); }
    if (int i = logicalRect.height % PANGO_SCALE) { WARNING("Height remainder: " << i); }

    width  = logicalRect.width / PANGO_SCALE;
    height = logicalRect.height / PANGO_SCALE;
}
