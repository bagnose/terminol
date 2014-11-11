// vi:noai:sw=4
// Copyright © 2014 David Bryant

#ifndef XCB__WIDGET__HXX
#define XCB__WIDGET__HXX

#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/common.hxx"
#include "terminol/xcb/dispatcher.hxx"

#include <xcb/xcb.h>

class Widget :
    protected I_Dispatcher::I_Observer,
    protected Uncopyable
{
public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

protected:
    Widget(I_Dispatcher & dispatcher,
           Basics       & basics,
           uint32_t       background,
           int16_t x, int16_t y, uint16_t width, uint16_t height);

    ~Widget();

protected:
    void resize(const xcb_rectangle_t & geometry);

    void map();

    xcb_window_t getWindow() { return _window; }

private:
    I_Dispatcher  & _dispatcher;
    Basics        & _basics;
    xcb_window_t    _window;
};

#endif // XCB__WIDGET__HXX
