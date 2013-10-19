// vi:noai:sw=4

#ifndef XCB__WINDOW__HXX
#define XCB__WINDOW__HXX

#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/terminal.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pattern.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <cairo-xcb.h>
#include <cairo-ft.h>

class Window :
    protected Terminal::I_Observer,
    protected FontManager::I_Client,
    protected Uncopyable
{
public:
    class I_Observer {
    public:
        virtual void windowSync() throw () = 0;
        virtual void windowDefer(Window * window) throw () = 0;
        virtual void windowSelected(Window * window) throw () = 0;
        virtual void windowExited(Window * window, int exitCode) throw () = 0;

    protected:
        I_Observer() {}
        ~I_Observer() {}
    };

private:
    I_Observer      & _observer;
    const Config    & _config;
    Basics          & _basics;
    const ColorSet  & _colorSet;
    FontManager     & _fontManager;
    FontSet         * _fontSet;
    xcb_window_t      _window;
    bool              _destroyed;
    xcb_gcontext_t    _gc;
    xcb_rectangle_t   _geometry;
    xcb_rectangle_t   _deferredGeometry;
    Terminal        * _terminal;
    bool              _open;
    HPos              _pointerPos;

    bool              _mapped;          // Is the window mapped? If so then _pixmap and _surface are good.
    xcb_pixmap_t      _pixmap;          // Created when mapped, destroyed when unmapped.
    cairo_surface_t * _surface;

    cairo_t         * _cr;              // Cairo drawing context. Created only as required.

    std::string       _title;
    std::string       _icon;

    std::string       _primarySelection;
    std::string       _clipboardSelection;

    bool              _pressed;         // Is there an active button press?
    int               _pressCount;      // single, double, triple-click, etc
    xcb_timestamp_t   _lastPressTime;
    xcb_button_t      _button;

    bool              _cursorVisible;

    bool              _deferralsAllowed;
    bool              _deferred;

    bool              _transientTitle;
    bool              _hadDeleteRequest;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Window(I_Observer         & observer,
           const Config       & config,
           I_Selector         & selector,
           I_Deduper          & deduper,
           Basics             & basics,
           const ColorSet     & colorSet,
           FontManager        & fontManager,
           const Tty::Command & command = Tty::Command()) throw (Error);

    virtual ~Window();

    xcb_window_t getWindowId() { return _window; }

    // Events:

    void keyPress(xcb_key_press_event_t * event);
    void keyRelease(xcb_key_release_event_t * event);
    void buttonPress(xcb_button_press_event_t * event);
    void buttonRelease(xcb_button_release_event_t * event);
    void motionNotify(xcb_motion_notify_event_t * event);
    void mapNotify(xcb_map_notify_event_t * event);
    void unmapNotify(xcb_unmap_notify_event_t * event);
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
    void clientMessage(xcb_client_message_event_t * event);

    //

    void tryReap();
    void clearSelection();
    void deferral();

protected:
    void icccmConfigure();

    void pos2XY(Pos pos, int & x, int & y) const;
    bool xy2Pos(int x, int y, HPos & pos) const;

    void updateTitle();
    void updateIcon();

    void setTitle(const std::string & title);

    void createPixmapAndSurface();
    void destroySurfaceAndPixmap();
    void draw();
    void drawBorder();
    void copy(int x, int y, int w, int h);

    void handleConfigure();
    void handleResize();
    void handleMove();
    void resizeToAccommodate(int16_t rows, int16_t cols, bool sync);

    void sizeToRowsCols(int16_t & rows, int16_t & cols) const;

    void handleDelete();

    void cursorVisibility(bool visible);

    // Terminal::I_Observer implementation:

    void terminalGetDisplay(std::string & display) throw () override;
    void terminalCopy(const std::string & text, Terminal::Selection selection) throw () override;
    void terminalPaste(Terminal::Selection selection) throw () override;
    void terminalResizeLocalFont(int delta) throw () override;
    void terminalResizeGlobalFont(int delta) throw () override;
    void terminalResetTitleAndIcon() throw () override;
    void terminalSetWindowTitle(const std::string & str) throw () override;
    void terminalSetIconName(const std::string & str) throw () override;
    void terminalBell() throw () override;
    void terminalResizeBuffer(int16_t rows, int16_t cols) throw () override;
    bool terminalFixDamageBegin() throw () override;
    void terminalDrawBg(Pos     pos,
                        int16_t count,
                        UColor color) throw () override;
    void terminalDrawFg(Pos             pos,
                        int16_t         count,
                        UColor          color,
                        AttrSet         attrs,
                        const uint8_t * str,
                        size_t          size) throw () override;
    void terminalDrawCursor(Pos             pos,
                            UColor          fg,
                            UColor          bg,
                            AttrSet         attrs,
                            const uint8_t * str,
                            size_t          size,
                            bool            wrapNext,
                            bool            focused) throw () override;
    void terminalDrawScrollbar(size_t  totalRows,
                               size_t  historyOffset,
                               int16_t visibleRows) throw () override;
    void terminalFixDamageEnd(const Region & damage,
                              bool           scrollbar) throw () override;
    void terminalChildExited(int exitStatus) throw () override;

    // FontManager::I_Client implementation:

    void useFontSet(FontSet * fontSet, int delta) throw () override;

private:
    XColor getColor(const UColor & ucolor) const {
        switch (ucolor.type) {
            case UColor::Type::STOCK:
                switch (ucolor.name) {
                    case UColor::Name::TEXT_FG:
                        return _colorSet.getNormalFgColor();
                    case UColor::Name::TEXT_BG:
                        return _colorSet.getNormalBgColor();
                    case UColor::Name::SELECT_FG:
                        return _colorSet.getSelectFgColor();
                    case UColor::Name::SELECT_BG:
                        return _colorSet.getSelectBgColor();
                    case UColor::Name::CURSOR_FILL:
                        return _colorSet.getCursorFillColor();
                    case UColor::Name::CURSOR_TEXT:
                        return _colorSet.getCursorTextColor();
                }

                FATAL("Unreachable");
            case UColor::Type::INDEXED:
                return _colorSet.getIndexedColor(ucolor.index);
            case UColor::Type::DIRECT:
                auto v = ucolor.values;
                auto d = 255.0;
                return XColor(v.r / d, v.g / d, v.b / d);
        }

        FATAL("Unreachable");
    }
};

#endif // XCB__WINDOW__HXX
