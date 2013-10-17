// vi:noai:sw=4

#ifndef SUPPORT__COLOR__HXX
#define SUPPORT__COLOR__HXX

#include <cmath>

void hsv_to_rgb(double & h, double & s, double & v) {
    if (s == 0.0) {
        h = v;
        s = v;
    }
    else {
        auto hue        = clamp(h * 6.0, 0.0, 6.0);
        auto saturation = s;
        auto value      = v;

        if (hue == 6.0) { hue = 0.0; }

        auto f = hue - std::floor(hue);
        auto p = value * (1.0 - saturation);
        auto q = value * (1.0 - saturation * f);
        auto t = value * (1.0 - saturation * (1.0 - f));

        switch (static_cast<int>(hue)) {
            case 0:
                h = value;
                s = t;
                v = p;
                break;
            case 1:
                h = q;
                s = value;
                v = p;
                break;
            case 2:
                h = p;
                s = value;
                v = t;
                break;
            case 3:
                h = p;
                s = q;
                v = value;
                break;
            case 4:
                h = t;
                s = p;
                v = value;
                break;
            case 5:
                h = value;
                s = p;
                v = q;
                break;
            default:
                FATAL("unsupported colour");
        }
    }
}

void rgb_to_hsv(double & r, double & g, double & b) {
    double h, s, v;
    double min, max;
    double delta;

    auto red   = r;
    auto green = g;
    auto blue  = b;

    h = 0.0;

    if (red > green) {
        if (red > blue) {
            max = red;
        }
        else {
            max = blue;
        }

        if (green < blue) {
            min = green;
        }
        else {
            min = blue;
        }
    }
    else {
        if (green > blue) {
            max = green;
        }
        else {
            max = blue;
        }

        if (red < blue) {
            min = red;
        }
        else {
            min = blue;
        }
    }

    v = max;

    if (max != 0.0) {
        s = (max - min) / max;
    }
    else {
        s = 0.0;
    }

    if (s == 0.0) {
        h = 0.0;
    }
    else {
        delta = max - min;

        if (red == max) {
            h = (green - blue) / delta;
        }
        else if (green == max) {
            h = 2 + (blue - red) / delta;
        }
        else if (blue == max) {
            h = 4 + (red - green) / delta;
        }

        h /= 6.0;

        if (h < 0.0) {
            h += 1.0;
        }
        else if (h > 1.0) {
            h -= 1.0;
        }
    }

    r = h;
    g = s;
    b = v;
}

#endif // SUPPORT__COLOR__HXX
