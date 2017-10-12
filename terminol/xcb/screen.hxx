// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef XCB__SCREEN__HXX
#define XCB__SCREEN__HXX

#include "terminol/xcb/basics.hxx"
#include "terminol/xcb/color_set.hxx"
#include "terminol/xcb/font_manager.hxx"
#include "terminol/xcb/widget.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/terminal.hxx"
#include "terminol/support/async_destroyer.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pattern.hxx"

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <cairo-xcb.h>

class Screen final :
    public    Widget,
    protected Terminal::I_Observer,
    protected FontManager::I_Client
{
public:
    class I_Observer {
    public:
        virtual void screenSync() = 0;
        virtual void screenDefer(Screen * screen) = 0;
        virtual void screenSelected(Screen * screen) = 0;
        virtual void screenReaped(Screen * screen, int status) = 0;

    protected:
        ~I_Observer() = default;
    };

private:
    I_Observer      & _observer;
    const Config    & _config;
    Basics          & _basics;
    const ColorSet  & _colorSet;
    FontManager     & _fontManager;
    FontSet         * _fontSet;
    bool              _destroyed;
    xcb_gcontext_t    _gc;
    xcb_rectangle_t   _geometry;            // Note x/y is wrt root window.
    xcb_rectangle_t   _deferredGeometry;    // Note x/y is wrt root window.
    Terminal        * _terminal;
    bool              _open;
    Pos               _pointerPos;

    // If the window is mapped then _pixmap and _surface are be valid. Otherwise not.
    bool              _mapped;
    xcb_pixmap_t      _pixmap;              // Created when mapped, destroyed when unmapped.
    cairo_surface_t * _surface;             // Ditto.

    cairo_t         * _cr;                  // Cairo drawing context. Created only as required.

    enum class Entitlement {
        PERMANENT,
        TRANSIENT,
        PENDING
    };

    Entitlement       _entitlement;
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

    bool              _hadDeleteRequest;

public:
    struct Error {
        explicit Error(const std::string & message_) : message(message_) {}
        std::string message;
    };

    Screen(I_Observer         & observer,
           const Config       & config,
           I_Selector         & selector,
           I_Deduper          & deduper,
           AsyncDestroyer     & destroyer,
           I_Dispatcher       & dispatcher,
           Basics             & basics,
           const ColorSet     & colorSet,
           FontManager        & fontManager,
           const Tty::Command & command = Tty::Command()) /*throw (Widget::Error, Error)*/;

    ~Screen();

    xcb_window_t getWindowId() { return getWindow(); }

    //

    void redraw();
    void tryReap();
    void killReap();
    void clearSelection();
    void deferral();

protected:
    void icccmConfigure();

    void pos2XY(Pos pos, int & x, int & y) const;
    bool xy2Pos(int x, int y, Pos & pos, Hand & hand) const;

    void setTitle(const std::string & title, bool prependGeometry);
    void setIcon(const std::string & icon);

    void createPixmapAndSurface();
    void destroySurfaceAndPixmap();
    void renderPixmap();
    void drawBorder();
    void copyPixmapToWindow(int x, int y, int w, int h);

    void handleConfigure();
    void handleResize();
    void handleMove();
    void resizeToAccommodate(int16_t rows, int16_t cols, bool sync);

    void sizeToRowsCols(int16_t & rows, int16_t & cols) const;

    void handleDelete();

    void cursorVisibility(bool visible);

    // Terminal::I_Observer implementation:

    const std::string & terminalGetDisplayName() const override;
    void terminalCopy(const std::string & text, Terminal::Selection selection) override;
    void terminalPaste(Terminal::Selection selection) override;
    void terminalResizeLocalFont(int delta) override;
    void terminalResizeGlobalFont(int delta) override;
    void terminalResetTitleAndIcon() override;
    void terminalSetWindowTitle(const std::string & str, bool transient) override;
    void terminalSetIconName(const std::string & str) override;
    void terminalBell() override;
    void terminalResizeBuffer(int16_t rows, int16_t cols) override;
    bool terminalFixDamageBegin() override;
    void terminalDrawBg(Pos     pos,
                        int16_t count,
                        UColor color) override;
    void terminalDrawFg(Pos             pos,
                        int16_t         count,
                        UColor          color,
                        AttrSet         attrs,
                        const uint8_t * str,
                        size_t          size) override;
    void terminalDrawCursor(Pos             pos,
                            UColor          fg,
                            UColor          bg,
                            AttrSet         attrs,
                            const uint8_t * str,
                            size_t          size,
                            bool            wrapNext,
                            bool            focused) override;
    void terminalDrawScrollbar(size_t  totalRows,
                               size_t  historyOffset,
                               int16_t visibleRows) override;
    void terminalFixDamageEnd(const Region & damage,
                              bool           scrollbar) override;
    void terminalReaped(int exitStatus) override;

    // FontManager::I_Client implementation:

    void useFontSet(FontSet * fontSet, int delta) override;

    // I_Dispatcher::I_Observer overrides:

    void keyPress(xcb_key_press_event_t * event) noexcept override;
    void keyRelease(xcb_key_release_event_t * event) noexcept override;
    void buttonPress(xcb_button_press_event_t * event) noexcept override;
    void buttonRelease(xcb_button_release_event_t * event) noexcept override;
    void motionNotify(xcb_motion_notify_event_t * event) noexcept override;
    void mapNotify(xcb_map_notify_event_t * event) noexcept override;
    void unmapNotify(xcb_unmap_notify_event_t * event) noexcept override;
    void expose(xcb_expose_event_t * event) noexcept override;
    void configureNotify(xcb_configure_notify_event_t * event) noexcept override;
    void focusIn(xcb_focus_in_event_t * event) noexcept override;
    void focusOut(xcb_focus_out_event_t * event) noexcept override;
    void enterNotify(xcb_enter_notify_event_t * event) noexcept override;
    void leaveNotify(xcb_leave_notify_event_t * event) noexcept override;
    void destroyNotify(xcb_destroy_notify_event_t * event) noexcept override;
    void selectionClear(xcb_selection_clear_event_t * event) noexcept override;
    void selectionNotify(xcb_selection_notify_event_t * event) noexcept override;
    void selectionRequest(xcb_selection_request_event_t * event) noexcept override;
    void clientMessage(xcb_client_message_event_t * event) noexcept override;

private:
    DColor getColor(const UColor & ucolor) const {
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
                return DColor(v.r / d, v.g / d, v.b / d);
        }

        FATAL("Unreachable");
    }
};

#endif // XCB__SCREEN__HXX
