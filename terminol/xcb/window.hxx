// vi:noai:sw=4

#ifndef XCB__WINDOW__HXX
#define XCB__WINDOW__HXX

#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_set.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/tty.hxx"
#include "terminol/common/terminal.hxx"
#include "terminol/common/support.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <cairo-xcb.h>
#include <cairo-ft.h>

class Window :
    protected Terminal::I_Observer,
    protected Uncopyable
{
    static const int         BORDER_THICKNESS;
    static const int         SCROLLBAR_WIDTH;
    static const std::string DEFAULT_TITLE;

    Basics          & _basics;
    const ColorSet  & _colorSet;
    FontSet         & _fontSet;
    xcb_window_t      _window;
    xcb_gcontext_t    _gc;
    uint16_t          _width;           // Actual width/height of window
    uint16_t          _height;
    uint16_t          _nominalWidth;    // Used width/height of window
    uint16_t          _nominalHeight;
    Tty             * _tty;
    Terminal        * _terminal;
    bool              _isOpen;
    uint16_t          _pointerRow;
    uint16_t          _pointerCol;
    bool              _mapped;          // Is the window mapped.

    bool              _doubleBuffer;
    xcb_pixmap_t      _pixmap;          // Created when mapped, destroyed when unmapped.

    cairo_surface_t * _surface;

    cairo_t         * _cr;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Window(Basics             & basics,
           const ColorSet     & colorSet,
           FontSet            & fontSet,
           const KeyMap       & keyMap,
           const std::string  & term,
           const Tty::Command & command,
           bool                 doubleBuffer,
           bool                 trace,
           bool                 sync) throw (Error);

    virtual ~Window();

    // We handle these:

    bool isOpen() const { return _isOpen; }
    int  getFd() { ASSERT(_isOpen, ""); return _tty->getFd(); }

    // The following calls are forwarded to the Terminal:

    void read();
    bool needsFlush() const;
    void flush();

    // Events:

    void keyPress(xcb_key_press_event_t * event);
    void keyRelease(xcb_key_release_event_t * event);
    void buttonPress(xcb_button_press_event_t * event);
    void buttonRelease(xcb_button_release_event_t * event);
    void motionNotify(xcb_motion_notify_event_t * event);
    void mapNotify(xcb_map_notify_event_t * event);
    void unmapNotify(xcb_unmap_notify_event_t * event);
    void reparentNotify(xcb_reparent_notify_event_t * event);
    void expose(xcb_expose_event_t * event);
    void configureNotify(xcb_configure_notify_event_t * event);
    void focusIn(xcb_focus_in_event_t * event);
    void focusOut(xcb_focus_out_event_t * event);
    void enterNotify(xcb_enter_notify_event_t * event);
    void leaveNotify(xcb_leave_notify_event_t * event);
    void visibilityNotify(xcb_visibility_notify_event_t * event);
    void destroyNotify(xcb_destroy_notify_event_t * event);

protected:
    void icccmConfigure();

    void rowCol2XY(uint16_t row, uint16_t col, int & x, int & y) const;
    bool xy2RowCol(int x, int y, uint16_t & row, uint16_t & col) const;

    void setTitle(const std::string & title);

    void draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void drawBorder();
    void drawScrollBar();

    // Terminal::I_Observer implementation:

    void terminalResetTitle() throw ();
    void terminalSetTitle(const std::string & title) throw ();
    bool terminalBeginFixDamage(bool internal) throw ();
    void terminalDrawRun(uint16_t       row,
                         uint16_t       col,
                         uint8_t        fg,
                         uint8_t        bg,
                         AttributeSet   attrs,
                         const char   * str,
                         size_t         count) throw ();
    void terminalDrawCursor(uint16_t       row,
                            uint16_t       col,
                            uint8_t        fg,
                            uint8_t        bg,
                            AttributeSet   attrs,
                            const char   * str,
                            bool           special) throw ();
    void terminalEndFixDamage(bool internal) throw ();
    void terminalChildExited(int exitStatus) throw ();
};

#endif // XCB__WINDOW__HXX
