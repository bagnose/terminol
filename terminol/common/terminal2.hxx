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

    const ModeSet      & getModes()  const { return _modes;     }       // Try to make non-public
    const SimpleBuffer & buffer()    const { return _buffer;    }
    uint16_t             cursorRow() const { return _cursorRow; }
    uint16_t             cursorCol() const { return _cursorCol; }
    void                 resize(uint16_t rows, uint16_t cols);

    void read();
    void write(const char * data, size_t size);
    bool areWritesQueued() const;
    void flush();

protected:

    //void clearLine(ClearLine clear) throw ();
    //void clearScreen(ClearScreen clear) throw ();
    //void insertChars(uint16_t num) throw ();
    //void insertLines(uint16_t num) throw ();
    //void deleteLines(uint16_t num) throw ();
    //void resetFg() throw ();
    //void resetBg() throw ();
    //void setFg(uint8_t fg) throw ();
    //void setBg(uint8_t bg) throw ();
    //void clearAttributes() throw ();
    //void setAttribute(Attribute attribute, bool value) throw ();
    //void setMode(Mode mode, bool value) throw ();
    //void setTabStop() throw ();
    //void advanceTab(uint16_t count) throw ();
    //void setScrollTopBottom(uint16_t row0, uint16_t row1);
    //void setScrollTop(uint16_t row);
    void resetAll() throw ();
    //void setTitle(const std::string & title) throw ();
    //void utf8(const char * s, size_t count, size_t size) throw ();
    //void getCursorPos(uint16_t & row, uint16_t & col) const throw ();

    void processRead(const char * data, size_t size);
    void processChar(utf8::Seq seq, utf8::Length length);
    void processControl(char c);
    void processNormal(utf8::Seq seq, utf8::Length length);
    void processEscape(char c);
    void processCsi(const std::vector<char> & seq);
    void processDcs(const std::vector<char> & seq);
    void processOsc(const std::vector<char> & seq);
    void processSpecial(const std::vector<char> & seq);
    void processAttributes(const std::vector<int32_t> & args);
    void processModes(bool priv, bool set, const std::vector<int32_t> & args);
};

#endif // TERMINAL2__HXX
