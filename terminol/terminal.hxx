// vi:noai:sw=4

#ifndef TERMINAL__HXX
#define TERMINAL__HXX

#include "terminol/tty.hxx"
#include "terminol/bit_sets.hxx"
#include "terminol/simple_buffer.hxx"

#include <vector>

class Terminal : protected Tty::IObserver {
public:
    class IObserver {
    public:
        //virtual void terminalDamageRows(uint16_t row, uint16_t col, uint16_t len) = 0;
        //virtual void terminalDamage(uint16_t row, uint16_t col) throw () = 0;
        //virtual void terminalDamageRange(uint16_t row, uint16_t col) throw () = 0;

        virtual void terminalBegin() throw () = 0;
        virtual void terminalDamageChars(uint16_t row,
                                         uint16_t col0, uint16_t col1) throw () = 0;
        virtual void terminalDamageAll() throw () = 0;
        virtual void terminalResetTitle() throw () = 0;
        virtual void terminalSetTitle(const std::string & title) throw () = 0;
        virtual void terminalEnd() throw () = 0;
        virtual void terminalChildExited(int exitStatus) throw () = 0;

    protected:
        IObserver() {}
        virtual ~IObserver() {}
    };

private:
    IObserver         & _observer;
    bool                _dispatch;
    SimpleBuffer        _buffer;
    uint16_t            _cursorRow;
    uint16_t            _cursorCol;
    uint8_t             _bg;
    uint8_t             _fg;
    AttributeSet        _attributes;
    ModeSet             _modes;
    std::vector<bool>   _tabs;
    Tty                 _tty;

public:
    Terminal(IObserver          & observer,
             uint16_t             rows,
             uint16_t             cols,
             const std::string  & windowId,
             const std::string  & term,
             const Tty::Command & command);

    virtual ~Terminal();

    const ModeSet      & getModes()  const { return _modes;     }
    const SimpleBuffer & buffer()    const { return _buffer;    }
    uint16_t             cursorRow() const { return _cursorRow; }
    uint16_t             cursorCol() const { return _cursorCol; }

    // TODO buffer access through scroll state.

    bool isOpen() const { return _tty.isOpen(); }
    int  getFd() { return _tty.getFd(); }
    void read() { ASSERT(!_dispatch, ""); _tty.read(); }
    void enqueueWrite(const char * data, size_t size) { ASSERT(!_dispatch, ""); _tty.enqueueWrite(data, size); }
    bool isWritePending() const { ASSERT(!_dispatch, ""); return _tty.isWritePending(); }
    void write() { ASSERT(!_dispatch, ""); _tty.write(); }
    void resize(uint16_t rows, uint16_t cols);

protected:

    // Tty::IObserver implementation:

    void ttyBegin() throw ();
    void ttyControl(Control control) throw ();
    void ttyMoveCursor(uint16_t row, uint16_t col) throw ();
    void ttyRelMoveCursor(int16_t dRow, int16_t dCol) throw ();
    void ttyClearLine(ClearLine clear) throw ();
    void ttyClearScreen(ClearScreen clear) throw ();
    void ttyInsertLines(uint16_t num) throw ();
    void ttyDeleteLines(uint16_t num) throw ();
    void ttySetFg(uint8_t fg) throw ();
    void ttySetBg(uint8_t bg) throw ();
    void ttyClearAttributes() throw ();
    void ttySetAttribute(Attribute attribute, bool value) throw ();
    void ttySetMode(Mode mode, bool value) throw ();
    void ttySetTabStop() throw ();
    void ttyReset() throw ();
    void ttyResetTitle() throw ();
    void ttySetTitle(const std::string & title) throw ();
    void ttyUtf8(const char * s, utf8::Length length) throw ();
    void ttyEnd() throw ();
    void ttyGetCursorPos(uint16_t & row, uint16_t & col) const throw ();

    void ttyChildExited(int exitCode) throw ();
};

#endif // TERMINAL__HXX
