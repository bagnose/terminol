// vi:noai:sw=4

#include "terminol/xcb/font_set.hxx"
#include "terminol/support/pattern.hxx"

#include <cairo/cairo-xcb.h>
#include <pango/pangocairo.h>

FontSet::FontSet(const Config & config,
                 Basics       & basics) throw (Error) :
    _config(config),
    _basics(basics)
{
    const auto & name = config.fontName;
    int size = config.fontSize;


    _normal = load(name, size, true, false, false);
    auto normalGuard = scopeGuard([&] { unload(_normal); });
    _bold = load(name, size, false, true, false);
    auto boldGuard = scopeGuard([&] { unload(_bold); });
    _italic = load(name, size, false, false, true);
    auto italicGuard = scopeGuard([&] { unload(_italic); });
    _italicBold = load(name, size, false, true, true);
    auto italicBoldGuard = scopeGuard([&] { unload(_italicBold); });

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

PangoFontDescription * FontSet::load(const std::string & family,
                                     int                 size,
                                     bool                master,
                                     bool                bold,
                                     bool                italic) throw (Error) {
    {
    auto str = _config.fontName;
    str = "";
    }

    auto desc = pango_font_description_from_string(family.c_str());
    if (!desc) { throw Error("Failed to load font: " + family); }
    auto descGuard = scopeGuard([&] { pango_font_description_free(desc); });
    pango_font_description_set_absolute_size(desc, size * PANGO_SCALE);
    pango_font_description_set_weight(desc, bold ? PANGO_WEIGHT_BOLD : PANGO_WEIGHT_NORMAL);
    pango_font_description_set_style(desc, italic ? PANGO_STYLE_OBLIQUE : PANGO_STYLE_NORMAL);

    auto surface = cairo_xcb_surface_create(_basics.connection(),
                                            _basics.screen()->root,
                                            _basics.visual(),
                                            1, 1);
    auto surfaceGuard = scopeGuard([&] { cairo_surface_destroy(surface); });

    auto cr = cairo_create(surface);
    auto crGuard = scopeGuard([&] { cairo_destroy(cr); });

    auto layout = pango_cairo_create_layout(cr);
    auto layoutGuard = scopeGuard([&] { g_object_unref(layout); });

    pango_layout_set_font_description(layout, desc);

    pango_layout_set_text(layout, "M", -1);
    pango_cairo_update_layout(cr, layout);  // Required?

    PangoRectangle inkRect, logicalRect;
    pango_layout_get_extents(layout, &inkRect, &logicalRect);

#if 0
    double d = PANGO_SCALE;
    PRINT("inkRect: " << inkRect.x/d << " " << inkRect.y/d << " " << inkRect.width/d << " " << inkRect.height/d);
    PRINT("logicalRect: " << logicalRect.x/d << " " << logicalRect.y/d << " " << logicalRect.width/d << " " << logicalRect.height/d);
#endif

    uint16_t width  = logicalRect.width  / PANGO_SCALE;
    uint16_t height = logicalRect.height / PANGO_SCALE;

    /*
    PRINT(family << " " <<
          (bold ? "bold" : "normal") << " " <<
          (italic ? "italic" : "normal") << " " <<
          "WxH: " << width << "x" << height << std::endl);
          */

    if (master) {
        _width  = width;
        _height = height;
    }
    else {
        if (_width != width || _height != height) {
            FATAL("Size mismatch");
        }
    }

    descGuard.dismiss();

    return desc;
}

void FontSet::unload(PangoFontDescription * desc) {
    pango_font_description_free(desc);
}
