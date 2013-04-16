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
                   XIM                  xim,
                   const X_ColorSet   & colorSet,
                   const X_KeyMap     & keyMap,
                   X_FontSet          & fontSet,
                   const std::string  & term,
                   const Interlocutor::Command & command) throw (Error) :
    _display(display),
    _screen(screen),
    _colorSet(colorSet),
    _keyMap(keyMap),
    _fontSet(fontSet),
    _damage(false),
    _window(0),
    _xic(nullptr),
    _gc(0),
    _width(0),
    _height(0),
    _tty(nullptr),
    _terminal(nullptr),
    _isOpen(false),
    _pixmap(0),
    _pointerRow(std::numeric_limits<uint16_t>::max()),
    _pointerCol(std::numeric_limits<uint16_t>::min())
{
    XSetWindowAttributes attributes;
    attributes.background_pixel = XBlackPixelOfScreen(_screen);

    //uint16_t rows = 25;
    //uint16_t cols = 80;
    uint16_t rows = 6;
    uint16_t cols = 14;

    _width  = 2 * BORDER_THICKNESS + cols * _fontSet.getWidth() + SCROLLBAR_WIDTH;
    _height = 2 * BORDER_THICKNESS + rows * _fontSet.getHeight();

    _window = XCreateWindow(_display,
                            parent,
                            0, 0,               // x,y
                            _width, _height,    // w,h
                            0,                  // border width
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
                 PointerMotionMask |
                 PointerMotionHintMask |
                 ButtonMotionMask |
                 VisibilityChangeMask |
                 FocusChangeMask
                 );

    setTitle(DEFAULT_TITLE);

    //

    _xic = XCreateIC(xim, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, _window, nullptr);
    if (!_xic) {
        throw Error("XCreateIC failed.");
    }

    //

    XGCValues gcValues;
    gcValues.graphics_exposures = False;
    _gc = XCreateGC(_display, _window, GCGraphicsExposures, &gcValues);

    XMapWindow(_display, _window);
    XFlush(_display);

    _tty = new Tty(rows, cols, stringify(_window), term, command);
    _terminal = new Terminal(*this, *_tty, rows, cols);
    _isOpen = true;
}

X_Window::~X_Window() {
    if (_pixmap) {
        XFreePixmap(_display, _pixmap);
    }

    delete _tty;
    delete _terminal;

    XDestroyIC(_xic);

    XFreeGC(_display, _gc);

    if (_window) {
        XDestroyWindow(_display, _window);
    }
}

void X_Window::keyPress(XKeyEvent & event) {
    PRINT(std::endl);
    uint8_t  state   = event.state;
    //uint16_t keycode = event.keycode;

    char   buffer[16];
    KeySym keySym = 0;

#if 0
    int len = XLookupString(&event, buffer, sizeof buffer, &keySym, nullptr);
#else
    Status status;
    int len = XmbLookupString(_xic, &event, buffer, sizeof buffer, &keySym, &status);

    switch (status) {
        case XBufferOverflow:
            // Couldn't fit it in buffer.
            FATAL("Buffer overflow.");
            break;
        case XLookupNone:
            ASSERT(len == 0, "");
            FATAL("XLookupNone");
            break;
        case XLookupChars:
            // Use buffer - keySym is not valid.
            PRINT("Cells");
            break;
        case XLookupKeySym:
            // Use keySym - buffer is not valid.
            ASSERT(len == 0, "");
            PRINT("Keysym");
            break;
        case XLookupBoth:
            PRINT("Both");
            break;
        default:
            FATAL("WTF");
            break;
    }
#endif

    std::string str = std::string(buffer, buffer + len);

    const ModeSet & modes = _terminal->getModes();
    _keyMap.lookup(keySym, state & ~Mod2Mask,
                   modes.get(MODE_APPKEYPAD),
                   modes.get(MODE_APPCURSOR),
                   modes.get(MODE_CRLF),
                   false,
                   str);

        /*
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
        PRINT(<< "keycode=" << keycode << " mask=(" << maskStr.str() << ") " <<
              " str='" << str << "'" << " len=" << len);
    }
              */

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

void X_Window::motionNotify(XMotionEvent & event) {
    int x, y;
    unsigned int state;

    if (event.is_hint) {
        Window root, child;
        int root_x, root_y;
        XQueryPointer(_display, _window, &root, &child,
                      &root_x, &root_y,
                      &x, &y,
                      &state);
    }
    else {
        x     = event.x;
        y     = event.y;
        state = event.state;
    }

    //PRINT("Motion x=" << x << ", y=" << y);

    uint16_t row, col;
    xy2RowCol(x, y, row, col);

    if (row != _pointerRow || col != _pointerCol) {
        _pointerRow = row;
        _pointerCol = col;
        PRINT("Motion row=" << _pointerRow << ", col=" << _pointerCol);
    }
}

void X_Window::mapNotify(XMapEvent & UNUSED(event)) {
    PRINT("Map");

    ASSERT(!_pixmap, "");
    _pixmap = XCreatePixmap(_display, _window, _width, _height,
                            XDefaultDepthOfScreen(_screen));
    XFillRectangle(_display, _pixmap, _gc, 0, 0, _width, _height);
}

void X_Window::unmapNotify(XUnmapEvent & UNUSED(event)) {
    PRINT("Unmap");

    ASSERT(_pixmap, "");
    XFreePixmap(_display, _pixmap);
    _pixmap = 0;
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
        // TODO copy pixmap to window (if we are the last user...)
        //draw(event.x, event.y, event.width, event.height);
        drawAll();
    }
}

void X_Window::configureNotify(XConfigureEvent & event) {
    PRINT("Configure: " <<
          event.width << "x" << event.height << expSignStr(event.x) << expSignStr(event.y));
    ASSERT(event.window == _window, "Which window?");
    ASSERT(event.width != 0 && event.height != 0, "Zero size?");

    // We are only interested in size changes.
    if (_width == event.width && _height == event.height) return;

    _width  = event.width;
    _height = event.height;

    if (_pixmap) {
        XFreePixmap(_display, _pixmap);
        _pixmap = XCreatePixmap(_display, _window, _width, _height,
                                XDefaultDepthOfScreen(_screen));
        XFillRectangle(_display, _pixmap, _gc, 0, 0, _width, _height);
    }

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

    if (_pixmap) {
        drawAll();
    }
}

void X_Window::focusIn(XFocusChangeEvent & event) {
    ASSERT(event.window == _window, "Which window?");
    //PRINT("Focus in: mode=" << event.mode << ", detail=" << event.detail);

    if (event.mode == NotifyGrab) {
        return;
    }

    XSetICFocus(_xic);
}

void X_Window::focusOut(XFocusChangeEvent & event) {
    ASSERT(event.window == _window, "Which window?");
    //PRINT("Focus out: mode=" << event.mode << ", detail=" << event.detail);
    if (event.mode == NotifyGrab) {
        return;
    }

    XUnsetICFocus(_xic);
}

void X_Window::enterNotify(XCrossingEvent & event) {
    ASSERT(event.window == _window, "Which window?");
    //PRINT("Enter notify");
}

void X_Window::leaveNotify(XCrossingEvent & event) {
    ASSERT(event.window == _window, "Which window?");
    //PRINT("Leave notify");
}

void X_Window::visibilityNotify(XVisibilityEvent & event) {
    ASSERT(event.window == _window, "Which window?");
    // state values:
    // VisibilityUnobscured             0
    // VisibilityPartiallyObscured      1
    // VisibilityFullyObscured          2       (free pixmap)
    std::string str;
    switch (event.state) {
        case VisibilityUnobscured:
            str = "Unobscured";
            break;
        case VisibilityPartiallyObscured:
            str = "Partially Obscured";
            break;
        case VisibilityFullyObscured:
            str = "Fully Obscured";
            break;
        default:
            FATAL("");
    }
    PRINT("Visibility change: " << str);
}

void X_Window::destroyNotify(XDestroyWindowEvent & event) {
    PRINT("Destroy");
    ASSERT(event.window == _window, "Which window?");
    _tty->close();
    _isOpen = false;
    _window = 0;
}

void X_Window::rowCol2XY(uint16_t row, uint16_t col, int & x, int & y) const {
    x = BORDER_THICKNESS + col * _fontSet.getWidth();
    y = BORDER_THICKNESS + row * _fontSet.getHeight();
}

bool X_Window::xy2RowCol(int x, int y, uint16_t & row, uint16_t & col) const {
    if (x < BORDER_THICKNESS || y < BORDER_THICKNESS) {
        // Too left or up.
        return false;
    }
    else if (x < _width - BORDER_THICKNESS && y < _height - BORDER_THICKNESS) {
        // NYI
        row = (y - BORDER_THICKNESS) / _fontSet.getHeight();
        col = (x - BORDER_THICKNESS) / _fontSet.getWidth();
        ASSERT(row < _terminal->buffer().getRows(), "");
        ASSERT(col < _terminal->buffer().getCols(), "");
        return true;
    }
    else {
        // Too right or down.
        return false;
    }
}

void X_Window::drawAll() {
    ASSERT(_pixmap, "");

    XFillRectangle(_display, _pixmap, _gc, 0, 0, _width, _height);

    // TODO only draw the damaged chars.

    XftDraw * xftDraw = XftDrawCreate(_display, _pixmap,
                                      XDefaultVisualOfScreen(_screen),
                                      XDefaultColormapOfScreen(_screen));

    drawBuffer(xftDraw);
    drawCursor(xftDraw);

    XftDrawDestroy(xftDraw);

    XCopyArea(_display, _pixmap, _window, _gc,
              0, 0, _width, _height, 0, 0);

    XFlush(_display);

    //dump(std::cout, _terminal->buffer());

    _damage = false;
}

void X_Window::drawBuffer(XftDraw * xftDraw) {
    // Declare buffer at the outer scope (rather than for each row) to
    // minimise alloc/free.
    std::vector<char> buffer;
    // Reserve the largest amount of memory we could require.
    buffer.reserve(_terminal->buffer().getCols() * utf8::LMAX);

    for (uint16_t r = 0; r != _terminal->buffer().getRows(); ++r) {
        uint16_t          cc = 0;
        uint8_t           fg = 0, bg = 0;
        AttributeSet      attrs;

        for (uint16_t c = 0; c != _terminal->buffer().getCols(); ++c) {
            const Cell & cell = _terminal->buffer().getCell(r, c);

            if (buffer.empty() || fg != cell.fg() || bg != cell.bg() || attrs != cell.attrs()) {
                if (!buffer.empty()) {
                    // flush buffer
                    drawUtf8(xftDraw, r, cc, fg, bg, attrs,
                             &buffer.front(), c - cc, buffer.size());
                    buffer.clear();
                }

                cc    = c;
                fg    = cell.fg();
                bg    = cell.bg();
                attrs = cell.attrs();

                utf8::Length len = utf8::leadLength(cell.lead());
                buffer.resize(len);
                std::copy(cell.bytes(), cell.bytes() + len, &buffer.front());
            }
            else {
                size_t oldSize = buffer.size();
                utf8::Length len = utf8::leadLength(cell.lead());
                buffer.resize(buffer.size() + len);
                std::copy(cell.bytes(), cell.bytes() + len, &buffer[oldSize]);
            }
        }

        if (!buffer.empty()) {
            // flush buffer
            drawUtf8(xftDraw, r, cc, fg, bg, attrs,
                     &buffer.front(),
                     _terminal->buffer().getCols() - cc,
                     buffer.size());
            buffer.clear();
        }
    }
}

void X_Window::drawCursor(XftDraw * xftDraw) {
    // Draw the cursor
    //PRINT("Cursor: " << _terminal->cursorRow() << " " << _terminal->cursorCol());

    uint16_t r = _terminal->cursorRow();
    uint16_t c = _terminal->cursorCol();

    const Cell & cell = _terminal->buffer().getCell(r, c);

    drawUtf8(xftDraw,
             r, c,
             cell.bg(), cell.fg(),      // Swap fg/bg for cursor.
             cell.attrs(),
             cell.bytes(), 1, utf8::leadLength(cell.lead()));
}

void X_Window::drawUtf8(XftDraw    * xftDraw,
                        uint16_t     row,
                        uint16_t     col,
                        uint8_t      fg,
                        uint8_t      bg,
                        AttributeSet attr,
                        const char * str,
                        size_t       count,
                        size_t       size) {
    //PRINT(count << ": <<" << std::string(str, str + size) << ">>");

    int x, y;
    rowCol2XY(row, col, x, y);

    const XftColor * fgColor = _colorSet.getIndexedColor(fg);
    const XftColor * bgColor = _colorSet.getIndexedColor(bg);

    if (attr.get(ATTRIBUTE_REVERSE)) {
        std::swap(fgColor, bgColor);
    }

    XftFont * font = _fontSet.get(attr.get(ATTRIBUTE_BOLD),
                                  attr.get(ATTRIBUTE_ITALIC));

    XftDrawRect(xftDraw,
                bgColor,
                x, y,
                count * _fontSet.getWidth(),
                _fontSet.getHeight());

    XftDrawStringUtf8(xftDraw,
                      fgColor,
                      font,
                      x,
                      y + _fontSet.getAscent(),
                      reinterpret_cast<const FcChar8 *>(str),
                      size);
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

void X_Window::terminalDamageCells(uint16_t UNUSED(row), uint16_t UNUSED(col0), uint16_t UNUSED(col1)) throw () {
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
    if (_damage && _pixmap) {
        drawAll();
    }
}

void X_Window::terminalChildExited(int exitStatus) throw () {
    PRINT("Child exited: " << exitStatus);
    _isOpen = false;
}
