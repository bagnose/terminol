// vi:noai:sw=4
// Copyright © 2014 David Bryant

#include "terminol/xcb/widget.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/common/parser.hxx"

class MyWidget : public Widget {
public:
    explicit MyWidget(Dispatcher     & dispatcher,
                      Basics         & basics,
                      const ColorSet & color_set) :
        Widget(dispatcher, basics,
               color_set.getBackgroundPixel(), 0  -1, -1, 100, 100)
    {
        map();
    }

protected:
    void expose(xcb_expose_event_t * UNUSED(event)) noexcept override {
        PRINT("Expose");
    }
};

int main(int, char *[]) {
    Config config;
    parseConfig(config);

    Basics     basics;
    ColorSet   color_set(config, basics);
    Selector   selector;
    Dispatcher dispatcher(selector, basics.connection());
    MyWidget   widget(dispatcher, basics, color_set);

    for (;;) {
        selector.animate();
    }
}
