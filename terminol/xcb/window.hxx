// vi:noai:sw=4

#ifndef XCB__WINDOW__HXX
#define XCB__WINDOW__HXX

#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_set.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/tty.hxx"
#include "terminol/common/terminal.hxx"
#include "terminol/support/pattern.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <cairo-xcb.h>
#include <cairo-ft.h>

class Window :
    protected Terminal::I_Observer,
    protected Uncopyable
{
    const Config    & _config;
    Basics          & _basics;
    const ColorSet  & _colorSet;
    FontSet         & _fontSet;
    xcb_window_t      _window;
    xcb_gcontext_t    _gc;
    uint16_t          _width;
    uint16_t          _height;
    Tty             * _tty;
    Terminal        * _terminal;
    bool              _isOpen;
    uint16_t          _pointerRow;
    uint16_t          _pointerCol;
    bool              _mapped;          // Is the window mapped.
    bool              _focussed;        // Is the window focussed.

    bool              _hadExpose;
    xcb_pixmap_t      _pixmap;          // Created when mapped, destroyed when unmapped.

    cairo_surface_t * _surface;

    cairo_t         * _cr;

    std::string       _title;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Window(const Config       & config,
           Basics             & basics,
           const ColorSet     & colorSet,
           FontSet            & fontSet,
           const KeyMap       & keyMap,
           const Tty::Command & command) throw (Error);

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

    void updateTitle();

    void draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void drawBorder();

    // Terminal::I_Observer implementation:

    void terminalResizeFont(int delta) throw ();
    void terminalResetTitle() throw ();
    void terminalSetTitle(const std::string & title) throw ();
    void terminalResizeBuffer(uint16_t rows, uint16_t cols) throw ();
    bool terminalFixDamageBegin(bool internal) throw ();
    void terminalDrawRun(uint16_t        row,
                         uint16_t        col,
                         uint16_t        fg,
                         uint16_t        bg,
                         AttributeSet    attrs,
                         const uint8_t * str,
                         size_t          count) throw ();
    void terminalDrawCursor(uint16_t        row,
                            uint16_t        col,
                            uint16_t        fg,
                            uint16_t        bg,
                            AttributeSet    attrs,
                            const uint8_t * str,
                            bool            special) throw ();
    void terminalDrawScrollbar(size_t   totalRows,
                               size_t   historyOffset,
                               uint16_t visibleRows) throw ();
    void terminalFixDamageEnd(bool     internal,
                              uint16_t rowBegin,
                              uint16_t rowEnd,
                              uint16_t colBegin,
                              uint16_t colEnd,
                              bool     scrollbar) throw ();
    void terminalChildExited(int exitStatus) throw ();
};

#endif // XCB__WINDOW__HXX
