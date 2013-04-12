// vi:noai:sw=4

#include "terminol/x_window.hxx"
#include "terminol/common.hxx"

#include <sstream>

#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>

const int         X_Window::BORDER_THICKNESS = 1;
const int         X_Window::SCROLLBAR_WIDTH  = 8;
const std::string X_Window::DEFAULT_TITLE    = "terminol";

X_Window::X_Window(Display            * display,
                   Window               parent,
                   Screen             * screen,
                   const X_ColorSet   & colorSet,
                   const X_KeyMap     & keyMap,
                   X_FontSet          & fontSet,
                   const std::string  & term,
                   const Interlocutor::Command & command) :
    _display(display),
    _screen(screen),
    _colorSet(colorSet),
    _keyMap(keyMap),
    _fontSet(fontSet),
    _damage(false),
    _window(0),
    _width(0),
    _height(0),
    _tty(0),
    _terminal(nullptr),
    _isOpen(false),
    _hadConfigure(false)
{
    XSetWindowAttributes attributes;
    attributes.background_pixel = XBlackPixelOfScreen(_screen);

    uint16_t rows = 25;
    uint16_t cols = 80;

    uint16_t width  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    uint16_t height = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();

    _window = XCreateWindow(_display,
                            parent,
                            0, 0,          // x,y
                            width, height, // w,h
                            0,             // border width
                            CopyFromParent,
                            InputOutput,
                            CopyFromParent,
                            CWBackPixel,
                            &attributes);

    XSetStandardProperties(_display, _window,
                           "terminol", "terminol",
                           None, nullptr, 0, nullptr);

    XSelectInput(_display, _window,
                 StructureNotifyMask |
                 ExposureMask |
                 ButtonPressMask |
                 KeyPressMask |
                 // exp:
                 KeyReleaseMask |
                 ButtonReleaseMask |
                 EnterWindowMask |
                 LeaveWindowMask |
                 PointerMotionMask |        // both?
                 PointerMotionHintMask |
                 VisibilityChangeMask |
                 FocusChangeMask
                 );

    setTitle(DEFAULT_TITLE);

    XMapWindow(_display, _window);

    XGCValues gcValues;
    gcValues.graphics_exposures = False;
    _gc = XCreateGC(_display, _window, GCGraphicsExposures, &gcValues);

    XFlush(_display);

    _tty = new Tty(rows, cols, stringify(_window), term, command);
    _terminal = new Terminal(*this, *_tty, rows, cols);
    _isOpen = true;
}

X_Window::~X_Window() {
    if (_hadConfigure) {
        XFreePixmap(_display, _pixmap);
    }

    delete _tty;
    delete _terminal;

    XFreeGC(_display, _gc);
    XDestroyWindow(_display, _window);
}

void X_Window::keyPress(XKeyEvent & event) {
    uint8_t  state   = event.state;
    //uint16_t keycode = event.keycode;

    char   buffer[16];
    KeySym keySym;
    int len = XLookupString(&event, buffer, sizeof buffer, &keySym, nullptr);
    std::string str = std::string(buffer, buffer + len);

    const ModeSet & modes = _terminal->getModes();
    _keyMap.lookup(keySym, state & ~Mod2Mask,
                   modes.get(MODE_APPKEYPAD),
                   modes.get(MODE_APPCURSOR),
                   modes.get(MODE_CRLF),
                   false,
                   str);

    {
        // Run xmodmap without any arguments to discover these.
        std::ostringstream maskStr;
        if (state & ShiftMask)   maskStr << " SHIFT";
        if (state & LockMask)    maskStr << " LOCK";
        if (state & ControlMask) maskStr << " CTRL";
        if (state & Mod1Mask)    maskStr << " ALT";
        if (state & Mod2Mask)    maskStr << " NUM";
        if (state & Mod3Mask)    maskStr << " MOD3";
        if (state & Mod4Mask)    maskStr << " WIN";
        if (state & Mod5Mask)    maskStr << " MOD5";
        /*
        PRINT(<< "keycode=" << keycode << " mask=(" << maskStr.str() << ") " <<
              " str='" << str << "'" << " len=" << len);
              */
    }

    if (!str.empty()) {
        _terminal->enqueueWrite(str.data(), str.size());
    }
}

void X_Window::keyRelease(XKeyEvent & UNUSED(event)) {
}

void X_Window::buttonPress(XButtonEvent & UNUSED(event)) {
    PRINT("Button press");
}

void X_Window::buttonRelease(XButtonEvent & UNUSED(event)) {
    PRINT("Button release");
}

void X_Window::motionNotify(XMotionEvent & UNUSED(event)) {
    PRINT("Motion");
}

void X_Window::mapNotify(XMapEvent & UNUSED(event)) {
    PRINT("Map");
}

void X_Window::unmapNotify(XUnmapEvent & UNUSED(event)) {
    PRINT("Unmap");
}

void X_Window::reparentNotify(XReparentEvent & UNUSED(event)) {
    PRINT("Reparent");
}

void X_Window::expose(XExposeEvent & event) {
    PRINT("Expose");
    ASSERT(event.window == _window, "Which window?");
    /*
       PRINT("Expose: " <<
       event.x << " " << event.y << " " <<
       event.width << " " << event.height);
       */

    if (event.count == 0) {
        //draw(event.x, event.y, event.width, event.height);
        draw(0, 0, _width, _height);
    }
}

void X_Window::configure(XConfigureEvent & event) {
    PRINT("Configure: " <<
          event.width << "x" << event.height << expSignStr(event.x) << expSignStr(event.y));
    ASSERT(event.window == _window, "Which window?");
    ASSERT(event.width != 0 && event.height != 0, "Zero size?");

    // We are only interested in size changes.
    if (_width == event.width && _height == event.height) return;

    if (_hadConfigure) {
        XFreePixmap(_display, _pixmap);
    }
    else {
        _hadConfigure = true;
    }

    _width  = event.width;
    _height = event.height;
    _pixmap = XCreatePixmap(_display, _window, _width, _height,
                            XDefaultDepthOfScreen(_screen));
    XFillRectangle(_display, _pixmap, _gc, 0, 0, _width, _height);

    uint16_t rows, cols;

    if (_width  > 2 * BORDER_THICKNESS + _fontSet.getWidth() + SCROLLBAR_WIDTH &&
        _height > 2 * BORDER_THICKNESS + _fontSet.getHeight())
    {
        uint16_t w = _width  - (2 * BORDER_THICKNESS + SCROLLBAR_WIDTH);
        uint16_t h = _height - (2 * BORDER_THICKNESS);

        rows = h / _fontSet.getHeight();
        cols = w / _fontSet.getWidth();
    }
    else {
        rows = cols = 1;
    }

    ASSERT(rows > 0 && cols > 0, "");

    _tty->resize(rows, cols);
    _terminal->resize(rows, cols);

    draw(0, 0, _width, _height);
}

void X_Window::focusIn(XFocusChangeEvent & UNUSED(event)) {
    //PRINT("Focus in: mode=" << event.mode << ", detail=" << event.detail);
}

void X_Window::focusOut(XFocusChangeEvent & UNUSED(event)) {
    //PRINT("Focus out: mode=" << event.mode << ", detail=" << event.detail);
}

void X_Window::enterNotify(XCrossingEvent & UNUSED(event)) {
    //PRINT("Enter notify");
}

void X_Window::leaveNotify(XCrossingEvent & UNUSED(event)) {
    //PRINT("Leave notify");
}

void X_Window::visibilityNotify(XVisibilityEvent & event) {
    // state values:
    // VisibilityUnobscured             0
    // VisibilityPartiallyObscured      1
    // VisibilityFullyObscured          2       (free pixmap)
    PRINT("Visibility change: state=" << event.state);
}

void X_Window::rowCol2XY(uint16_t row, size_t col,
                         uint16_t & x, uint16_t & y) const {
    x = BORDER_THICKNESS + col * _fontSet.getWidth();
    y = BORDER_THICKNESS + row * _fontSet.getHeight();
}

void X_Window::draw(uint16_t UNUSED(ix), uint16_t UNUSED(iy),
                    uint16_t UNUSED(iw), uint16_t UNUSED(ih)) {
    //XClearWindow(_display, _window);

    ASSERT(_hadConfigure, "");

    // TODO only draw the damaged chars.

    XftDraw * xftDraw = XftDrawCreate(_display, _pixmap,//_window,
                                      XDefaultVisualOfScreen(_screen),
                                      XDefaultColormapOfScreen(_screen));

    for (size_t r = 0; r != _terminal->buffer().getRows(); ++r) {
        for (size_t c = 0; c != _terminal->buffer().getCols(); ++c) {
            const Char & ch = _terminal->buffer().getChar(r, c);

            if (true /*!ch.isNull()*/) {    // XXX drawing all characters ATM :(
                // PRINT(<<ch);
                uint16_t x, y;
                rowCol2XY(r, c, x, y);

                const XftColor * fgColor = _colorSet.getIndexedColor(ch.fg());
                const XftColor * bgColor = _colorSet.getIndexedColor(ch.bg());

                if (ch.attributes().get(ATTRIBUTE_REVERSE)) {
                    std::swap(fgColor, bgColor);
                }

                XftFont * font = _fontSet.get(ch.attributes().get(ATTRIBUTE_BOLD),
                                              ch.attributes().get(ATTRIBUTE_ITALIC));

                XftDrawRect(xftDraw,
                            bgColor,
                            x, y,
                            _fontSet.getWidth(),
                            _fontSet.getHeight());

                XftDrawStringUtf8(xftDraw,
                                  fgColor,
                                  font,
                                  x,
                                  y + _fontSet.getAscent(),
                                  reinterpret_cast<const FcChar8 *>(ch.bytes()),
                                  utf8::leadLength(ch.bytes()[0]));
            }
        }
    }

    {
        uint16_t x, y;
        rowCol2XY(_terminal->cursorRow(), _terminal->cursorCol(), x, y);
        XftDrawStringUtf8(xftDraw,
                          _colorSet.getCursorColor(),
                          _fontSet.getNormal(),
                          x,
                          y + _fontSet.getAscent(),
                          (const FcChar8 *)"¶", 2); 
                          //(const FcChar8 *)"█", 3); 
    }

    XftDrawDestroy(xftDraw);

    XCopyArea(_display, _pixmap, _window, _gc,
              0, 0, _width, _height, 0, 0);

    XFlush(_display);
}

void X_Window::setTitle(const std::string & title) {
    char * t = const_cast<char *>(title.c_str());
    XTextProperty prop;
    Xutf8TextListToTextProperty(_display, &t, 1, XUTF8StringStyle, &prop);
    XSetWMName(_display, _window, &prop);
}

// Terminal::I_Observer implementation:

void X_Window::terminalBegin() throw () {
}

void X_Window::terminalDamageChars(uint16_t UNUSED(row), uint16_t UNUSED(col0), uint16_t UNUSED(col1)) throw () {
    _damage = true;
}

void X_Window::terminalDamageAll() throw () {
    _damage = true;
}

void X_Window::terminalResetTitle() throw () {
    setTitle(DEFAULT_TITLE);
}

void X_Window::terminalSetTitle(const std::string & title) throw () {
    //PRINT("Set title: " << title);
    setTitle(title);
}

void X_Window::terminalEnd() throw () {
    if (_damage) {
        draw(0, 0, _width, _height);
        _damage = false;
    }
}

void X_Window::terminalChildExited(int exitStatus) throw () {
    PRINT("Child exited: " << exitStatus);
    _isOpen = false;
}
