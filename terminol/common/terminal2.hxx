// vi:noai:sw=4

#ifndef TERMINAL2__HXX
#define TERMINAL2__HXX

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/bit_sets.hxx"
#include "terminol/common/simple_buffer.hxx"
#include "terminol/common/support.hxx"

class Terminal2 : protected Uncopyable {
public:
    class I_Observer {
    public:
        virtual void terminalBegin() throw () = 0;
        virtual void terminalDamageCells(uint16_t row,
                                         uint16_t col0, uint16_t col1) throw () = 0;
        virtual void terminalDamageAll() throw () = 0;
        virtual void terminalResetTitle() throw () = 0;
        virtual void terminalSetTitle(const std::string & title) throw () = 0;
        virtual void terminalChildExited(int exitStatus) throw () = 0;
        virtual void terminalEnd() throw () = 0;

    protected:
        ~I_Observer() {}
    };

private:
    I_Observer         & _observer;
    bool                _dispatch;

    //
    // XXX Terminal stuff
    //

    SimpleBuffer        _buffer;
    uint16_t            _cursorRow;
    uint16_t            _cursorCol;
    uint8_t             _bg;
    uint8_t             _fg;
    AttributeSet        _attrs;
    ModeSet             _modes;
    std::vector<bool>   _tabs;

    //
    // XXX Interlocutor stuff
    //

    enum class State {
        NORMAL,
        ESCAPE,
        DCS,
        CSI,
        OSC,
        INNER,
        IGNORE,
        SPECIAL
    };

    I_Tty              & _tty;
    bool                 _dumpWrites;

    std::vector<char>    _writeBuffer;      // Spillover if the TTY would block.

    State                _state;
    State                _outerState;
    utf8::Machine        _utf8Machine;
    std::vector<char>    _escSeq;

public:
    Terminal2(I_Observer & observer,
              I_Tty      & tty,
              uint16_t     rows,
              uint16_t     cols);
    virtual ~Terminal2();

    void resize(uint16_t rows, uint16_t cols);

    void read();
    void write(const char * data, size_t size);
    bool areWritesQueued() const;
    void flush();

protected:
    void processRead(const char * data, size_t size);
    void processChar(utf8::Seq seq, utf8::Length length);
    void processControl(char c);
    void processNormal(utf8::Seq seq, utf8::Length length);
    void processEscape(char c);
    void processCsi();
    void processDcs();
    void processOsc();
    void processSpecial();
};

#endif // TERMINAL2__HXX
