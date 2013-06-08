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
public:
    class I_Observer {
    public:
        virtual void sync() throw () = 0;

    protected:
        I_Observer() {}
        ~I_Observer() {}
    };

private:
    I_Observer      & _observer;
    const Config    & _config;
    Basics          & _basics;
    const ColorSet  & _colorSet;
    FontSet         & _fontSet;
    xcb_window_t      _window;
    bool              _destroyed;
    xcb_gcontext_t    _gc;
    uint16_t          _width;
    uint16_t          _height;
    Tty             * _tty;
    Terminal        * _terminal;
    bool              _open;
    Pos               _pointerPos;
    bool              _mapped;          // Is the window mapped.

    bool              _hadExpose;
    xcb_pixmap_t      _pixmap;          // Created when mapped, destroyed when unmapped.

    cairo_surface_t * _surface;

    cairo_t         * _cr;

    std::string       _title;

    std::string       _primarySelection;
    std::string       _clipboardSelection;

    bool              _pressed;
    int               _pressCount;
    xcb_timestamp_t   _lastPressTime;
    xcb_button_t      _button;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Window(I_Observer         & observer,
           const Config       & config,
           Deduper            & deduper,
           Basics             & basics,
           const ColorSet     & colorSet,
           FontSet            & fontSet,
           const KeyMap       & keyMap,
           const Tty::Command & command = Tty::Command()) throw (Error);

    virtual ~Window();

    xcb_window_t getWindowId() { return _window; }

    // We handle these:

    bool isOpen() const { return _open; }
    int  getFd() { ASSERT(_open, ""); return _tty->getFd(); }

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
    void selectionClear(xcb_selection_clear_event_t * event);
    void selectionNotify(xcb_selection_notify_event_t * event);
    void selectionRequest(xcb_selection_request_event_t * event);

protected:
    void icccmConfigure();

    void pos2XY(Pos pos, int & x, int & y) const;
    bool xy2Pos(int x, int y, Pos & pos) const;

    void updateTitle();

    void draw(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void drawBorder();

    // Terminal::I_Observer implementation:

    void terminalCopy(const std::string & text, bool clipboard) throw ();
    void terminalPaste(bool clipboard) throw ();
    void terminalResizeFont(int delta) throw ();
    void terminalResetTitle() throw ();
    void terminalSetTitle(const std::string & title) throw ();
    void terminalResizeBuffer(uint16_t rows, uint16_t cols) throw ();
    bool terminalFixDamageBegin(bool internal) throw ();
    void terminalDrawBg(Pos    pos,
                        UColor color,
                        size_t count) throw ();
    void terminalDrawFg(Pos    pos,
                        UColor          color,
                        AttrSet         attrs,
                        const uint8_t * str,
                        size_t          count) throw ();
    void terminalDrawCursor(Pos             pos,
                            UColor          fg,
                            UColor          bg,
                            AttrSet         attrs,
                            const uint8_t * str,
                            bool            wrapNext,
                            bool            focused) throw ();
    void terminalDrawSelection(Pos      begin,
                               Pos      end,
                               bool     topless,
                               bool     bottomless) throw ();
    void terminalDrawScrollbar(size_t   totalRows,
                               size_t   historyOffset,
                               uint16_t visibleRows) throw ();
    void terminalFixDamageEnd(bool     internal,
                              Pos      begin,
                              Pos      end,
                              bool     scrollbar) throw ();
    void terminalChildExited(int exitStatus) throw ();

private:
    XColor getColor(const UColor & ucolor) const {
        switch (ucolor.type) {
            case UColor::Type::FOREGROUND:
                return _colorSet.getForegroundColor();
            case UColor::Type::BACKGROUND:
                return _colorSet.getBackgroundColor();
            case UColor::Type::INDEXED:
                return _colorSet.getIndexedColor(ucolor.index);
            case UColor::Type::DIRECT:
                auto v = ucolor.values;
                auto d = 255.0;
                return XColor(v.r / d, v.g / d, v.b / d);
        }
    }
};

#endif // XCB__WINDOW__HXX
