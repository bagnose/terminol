// vi:noai:sw=4

#ifndef X_WINDOW_INTERFACE__HXX
#define X_WINDOW_INTERFACE__HXX

#include <X11/Xlib.h>

class I_X_Window {
public:
    virtual bool isOpen() const = 0;
    virtual int  getFd() = 0;
    virtual void read()  = 0;
    virtual bool isWritePending() const = 0;
    virtual void write() = 0;

    virtual void keyPress(XKeyEvent & event) = 0;
    virtual void keyRelease(XKeyEvent & event) = 0;
    virtual void buttonPress(XButtonEvent & event) = 0;
    virtual void buttonRelease(XButtonEvent & event) = 0;
    virtual void motionNotify(XMotionEvent & event) = 0;
    virtual void mapNotify(XMapEvent & event) = 0;
    virtual void unmapNotify(XUnmapEvent & event) = 0;
    virtual void reparentNotify(XReparentEvent & event) = 0;
    virtual void expose(XExposeEvent & event) = 0;
    virtual void configure(XConfigureEvent & event) = 0;
    virtual void focusIn(XFocusChangeEvent & event) = 0;
    virtual void focusOut(XFocusChangeEvent & event) = 0;
    virtual void enterNotify(XCrossingEvent & event) = 0;
    virtual void leaveNotify(XCrossingEvent & event) = 0;
    virtual void visibilityNotify(XVisibilityEvent & event) = 0;
    // TODO selection

protected:
    I_X_Window()          {}
    ~I_X_Window() {}
};

#endif // X_WINDOW_INTERFACE__HXX
