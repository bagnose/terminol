// vi:noai:sw=4

#ifndef TERMINAL__HXX
#define TERMINAL__HXX

#include "terminol/interlocutor.hxx"
#include "terminol/bit_sets.hxx"
#include "terminol/simple_buffer.hxx"

#include <vector>

class Terminal : protected Interlocutor::I_Observer {
public:
    class I_Observer {
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
        I_Observer() {}
        ~I_Observer() {}
    };

private:
    I_Observer         & _observer;
    bool                _dispatch;
    SimpleBuffer        _buffer;
    uint16_t            _cursorRow;
    uint16_t            _cursorCol;
    uint8_t             _bg;
    uint8_t             _fg;
    AttributeSet        _attributes;
    ModeSet             _modes;
    std::vector<bool>   _tabs;
    Interlocutor        _inter;

public:
    Terminal(I_Observer & observer,
             I_Tty      & tty,
             uint16_t     rows,
             uint16_t     cols);
    virtual ~Terminal();

    const ModeSet      & getModes()  const { return _modes;     }
    const SimpleBuffer & buffer()    const { return _buffer;    }
    uint16_t             cursorRow() const { return _cursorRow; }
    uint16_t             cursorCol() const { return _cursorCol; }

    void resize(uint16_t rows, uint16_t cols);

    // TODO buffer access through scroll state.

    void read() { ASSERT(!_dispatch, ""); _inter.read(); }
    void enqueueWrite(const char * data, size_t size) { ASSERT(!_dispatch, ""); _inter.enqueueWrite(data, size); }
    bool isWritePending() const { ASSERT(!_dispatch, ""); return _inter.isWritePending(); }
    void write() { ASSERT(!_dispatch, ""); _inter.write(); }

protected:

    // Tty::I_Observer implementation:

    void interBegin() throw ();
    void interControl(Control control) throw ();
    void interMoveCursor(uint16_t row, uint16_t col) throw ();
    void interRelMoveCursor(int16_t dRow, int16_t dCol) throw ();
    void interClearLine(ClearLine clear) throw ();
    void interClearScreen(ClearScreen clear) throw ();
    void interInsertLines(uint16_t num) throw ();
    void interDeleteLines(uint16_t num) throw ();
    void interSetFg(uint8_t fg) throw ();
    void interSetBg(uint8_t bg) throw ();
    void interClearAttributes() throw ();
    void interSetAttribute(Attribute attribute, bool value) throw ();
    void interSetMode(Mode mode, bool value) throw ();
    void interSetTabStop() throw ();
    void interReset() throw ();
    void interResetTitle() throw ();
    void interSetTitle(const std::string & title) throw ();
    void interUtf8(const char * s, utf8::Length length) throw ();
    void interEnd() throw ();
    void interGetCursorPos(uint16_t & row, uint16_t & col) const throw ();

    void interChildExited(int exitCode) throw ();
};

#endif // TERMINAL__HXX
