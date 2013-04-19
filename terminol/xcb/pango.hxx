// vi:noai:sw=4

#ifndef PANGO__HXX
#define PANGO__HXX

#include "terminol/common/support.hxx"

#include <pango/pango-font.h>

void pangoPlay() {
    PRINT("Loading font description");
    const char * str = "inconsolata:pixelsize=10";
    PangoFontDescription * desc = pango_font_description_from_string(str);
    ENFORCE(desc, "");

    PRINT("Loaded: " << pango_font_description_get_family(desc)
          << ", size: " << pango_font_description_get_size(desc) / PANGO_SCALE);
}

#endif // PANGO__HXX
