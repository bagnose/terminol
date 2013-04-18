// vi:noai:sw=4

#ifndef X_WINDOW__HXX
#define X_WINDOW__HXX

#include <X11/Xlib.h>

#include "terminol/xlib/x_basics.hxx"
#include "terminol/xlib/x_color_set.hxx"
#include "terminol/xlib/x_key_map.hxx"
#include "terminol/xlib/x_font_set.hxx"
#include "terminol/common/tty.hxx"
#include "terminol/common/terminal.hxx"

class X_Window :
    protected Terminal::I_Observer,
    protected Uncopyable
{
    static const int         BORDER_THICKNESS;
    static const int         SCROLLBAR_WIDTH;
    static const std::string DEFAULT_TITLE;

    Display          * _display;
    Screen           * _screen;
    const X_ColorSet & _colorSet;
    const X_KeyMap   & _keyMap;
    X_FontSet        & _fontSet;
    Window             _window;
    XIC                _xic;
    GC                 _gc;
    uint16_t           _width;     // px
    uint16_t           _height;    // px
    Tty              * _tty;
    Terminal         * _terminal;
    bool               _isOpen;
    uint16_t           _pointerRow;
    uint16_t           _pointerCol;
    bool               _damage;
    Pixmap             _pixmap;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    X_Window(Display                     * display,
             Window                        parent,
             Screen                      * screen,
             XIM                           xim,
             const X_ColorSet            & colorSet,
             const X_KeyMap              & keyMap,
             X_FontSet                   & fontSet,
             const std::string           & term,
             const Interlocutor::Command & command) throw (Error);

    virtual ~X_Window();

    // We handle these:

    bool isOpen() const { return _isOpen; }
    int  getFd() { return _tty->getFd(); }

    // The following calls are forwarded to the Terminal.

    void read()  { _terminal->read(); }
    bool areWritesQueued() const { return _terminal->areWritesQueued(); }
    void flush() { _terminal->flush(); }

    // Events:

    void keyPress(XKeyEvent & event);
    void keyRelease(XKeyEvent & event);
    void buttonPress(XButtonEvent & event);
    void buttonRelease(XButtonEvent & event);
    void motionNotify(XMotionEvent & event);
    void mapNotify(XMapEvent & event);
    void unmapNotify(XUnmapEvent & event);
    void reparentNotify(XReparentEvent & event);
    void expose(XExposeEvent & event);
    void configureNotify(XConfigureEvent & event);
    void focusIn(XFocusChangeEvent & event);
    void focusOut(XFocusChangeEvent & event);
    void enterNotify(XCrossingEvent & event);
    void leaveNotify(XCrossingEvent & event);
    void visibilityNotify(XVisibilityEvent & event);
    void destroyNotify(XDestroyWindowEvent & event);

protected:
    void rowCol2XY(uint16_t row, uint16_t col, int & x, int & y) const;
    bool xy2RowCol(int x, int y, uint16_t & row, uint16_t & col) const;

    void drawAll();
    void drawBuffer(XftDraw * xftDraw);
    void drawCursor(XftDraw * xftDraw);
    void drawUtf8(XftDraw * xftDraw,
                  uint16_t row, uint16_t col,
                  uint8_t fg, uint8_t bg,
                  AttributeSet attr,
                  const char * str, size_t count, size_t size);
    void setTitle(const std::string & title);

    // Terminal::I_Observer implementation:

    void terminalBegin() throw ();
    void terminalDamageCells(uint16_t row, uint16_t col0, uint16_t col1) throw ();
    void terminalDamageAll() throw ();
    void terminalResetTitle() throw ();
    void terminalSetTitle(const std::string & title) throw ();
    void terminalChildExited(int exitStatus) throw ();
    void terminalEnd() throw ();
};

#endif // X_WINDOW__HXX
