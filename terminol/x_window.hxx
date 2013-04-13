// vi:noai:sw=4

#ifndef X_WINDOW__HXX
#define X_WINDOW__HXX

#include "terminol/x_window_interface.hxx"
#include "terminol/x_color_set.hxx"
#include "terminol/x_key_map.hxx"
#include "terminol/x_font_set.hxx"
#include "terminol/tty.hxx"
#include "terminol/terminal.hxx"

class X_Window :
    public    I_X_Window,
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
    bool               _damage;
    Window             _window;
    XIC                _xic;
    GC                 _gc;
    uint16_t           _width;     // px
    uint16_t           _height;    // px
    Tty              * _tty;
    Terminal         * _terminal;
    bool               _isOpen;
    bool               _hadConfigure;
    Pixmap             _pixmap;

public:
    X_Window(Display            * display,
             Window               parent,
             Screen             * screen,
             XIM                  xim,
             const X_ColorSet   & colorSet,
             const X_KeyMap     & keyMap,
             X_FontSet          & fontSet,
             const std::string  & term,
             const Interlocutor::Command & command);

    virtual ~X_Window();

    //
    // I_X_Window implementation:
    //

    // We handle these:

    bool isOpen() const { return _isOpen; }
    int  getFd() { return _tty->getFd(); }

    // The following calls are forwarded to the Terminal.

    void read()  { _terminal->read(); }
    bool isWritePending() const { return _terminal->isWritePending(); }
    void write() { _terminal->write(); }

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
    void configure(XConfigureEvent & event);
    void focusIn(XFocusChangeEvent & event);
    void focusOut(XFocusChangeEvent & event);
    void enterNotify(XCrossingEvent & event);
    void leaveNotify(XCrossingEvent & event);
    void visibilityNotify(XVisibilityEvent & event);

protected:
    void rowCol2XY(size_t row, uint16_t col, uint16_t & x, uint16_t & y) const;

    void drawAll();
    void drawBuffer(XftDraw * xftDraw);
    void drawCursor(XftDraw * xftDraw);
    void drawUtf8(XftDraw * xftDraw,
                  uint16_t row, uint16_t col,
                  uint8_t fg, uint8_t bg,
                  AttributeSet attributes,
                  const char * str, size_t count, size_t size);
    void setTitle(const std::string & title);

    // Terminal::I_Observer implementation:

    void terminalBegin() throw ();
    void terminalDamageChars(uint16_t row, uint16_t col0, uint16_t col1) throw ();
    void terminalDamageAll() throw ();
    void terminalResetTitle() throw ();
    void terminalSetTitle(const std::string & title) throw ();
    void terminalEnd() throw ();
    void terminalChildExited(int exitStatus) throw ();
};

#endif // X_WINDOW__HXX
