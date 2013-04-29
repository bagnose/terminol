// vi:noai:sw=4

#ifndef COMMON__TERMINAL__HXX
#define COMMON__TERMINAL__HXX

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/bit_sets.hxx"
#include "terminol/common/simple_buffer.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/support.hxx"

#include <xkbcommon/xkbcommon.h>

class Terminal : protected Uncopyable {
public:
    class I_Observer {
    public:
        virtual void terminalResetTitle() throw () = 0;
        virtual void terminalSetTitle(const std::string & title) throw () = 0;
        virtual void terminalFixDamage() throw () = 0;
        virtual void terminalChildExited(int exitStatus) throw () = 0;

    protected:
        ~I_Observer() {}
    };

private:
    I_Observer          & _observer;
    bool                  _dispatch;

    //
    //
    //

    const KeyMap        & _keyMap;
    SimpleBuffer          _priBuffer;
    SimpleBuffer          _altBuffer;
    SimpleBuffer        * _buffer;
    uint16_t              _cursorRow;
    uint16_t              _cursorCol;
    uint8_t               _bg;
    uint8_t               _fg;
    AttributeSet          _attrs;
    bool                  _originMode;
    ModeSet               _modes;
    std::vector<bool>     _tabs;

    uint16_t              _savedCursorRow;
    uint16_t              _savedCursorCol;
    uint8_t               _savedFg;
    uint8_t               _savedBg;
    AttributeSet          _savedAttrs;
    bool                  _savedOriginMode;

    //
    //
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

    I_Tty               & _tty;

    bool                  _dumpWrites;
    std::vector<char>     _writeBuffer;      // Spillover if the TTY would block.

    State                 _state;
    State                 _outerState;
    utf8::Machine         _utf8Machine;
    std::vector<char>     _escSeq;

    bool                  _trace;

public:
    Terminal(I_Observer   & observer,
             const KeyMap & keyMap,
             I_Tty        & tty,
             uint16_t       rows,
             uint16_t       cols,
             bool           trace);
    virtual ~Terminal();

    const SimpleBuffer & buffer()    const { return *_buffer;   }
    uint16_t             cursorRow() const { return _cursorRow; }
    uint16_t             cursorCol() const { return _cursorCol; }
    void                 resize(uint16_t rows, uint16_t cols);

    void keyPress(xkb_keysym_t keySym, uint8_t state);
    void read();
    bool needsFlush() const;
    void flush();

protected:

    void write(const char * data, size_t size);

    void resetAll();

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

#endif // COMMON__TERMINAL__HXX
