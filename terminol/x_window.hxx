// vi:noai:sw=4

#ifndef X_WINDOW__HXX
#define X_WINDOW__HXX

#include "terminol/x_window_interface.hxx"
#include "terminol/x_color_set.hxx"
#include "terminol/x_font_set.hxx"
#include "terminol/terminal.hxx"

class X_Window :
    public    I_X_Window,
    protected Terminal::IObserver,
    protected Uncopyable
{
    static const int         BORDER_THICKNESS;
    static const int         SCROLLBAR_WIDTH;
    static const std::string DEFAULT_TITLE;

    Display    * _display;
    Screen     * _screen;
    X_ColorSet & _colorSet;
    X_FontSet  & _fontSet;
    bool         _damage;
    Window       _window;
    uint16_t     _width;     // px
    uint16_t     _height;    // px
    Terminal   * _terminal;

public:
    X_Window(Display            * display,
             Window               parent,
             Screen             * screen,
             X_ColorSet         & colorSet,
             X_FontSet          & fontSet,
             const Tty::Command & command);

    ~X_Window();

    //
    // I_X_Window implementation:
    //

    // The following calls are forwarded to the Terminal.

    bool isOpen() const { return _terminal->isOpen(); }
    int  getFd() { return _terminal->getFd(); }
    void read()  { _terminal->read(); }
    bool isWritePending() const { return _terminal->isWritePending(); }
    void write() { _terminal->write(); }

    // Events:

    void keyPress(XKeyEvent & event);
    void keyRelease(XKeyEvent & event);
    void buttonPress(XButtonEvent & event);
    void buttonRelease(XButtonEvent & event);
    void expose(XExposeEvent & event);
    void configure(XConfigureEvent & event);

protected:
    void rowCol2XY(uint16_t row, size_t col, uint16_t & x, uint16_t & y) const;

    void draw(uint16_t ix, uint16_t iy, uint16_t iw, uint16_t ih);
    void setTitle(const std::string & title);

    // Terminal::IObserver implementation:

    void terminalBegin() throw ();
    void terminalDamageAll() throw ();
    void terminalResetTitle() throw ();
    void terminalSetTitle(const std::string & title) throw ();
    void terminalEnd() throw ();
    void terminalChildExited(int exitStatus) throw ();
};

#endif // X_WINDOW__HXX
