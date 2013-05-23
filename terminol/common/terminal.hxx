// vi:noai:sw=4

#ifndef COMMON__TERMINAL__HXX
#define COMMON__TERMINAL__HXX

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/vt_state_machine.hxx"
#include "terminol/common/vt_state_machine2.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/bit_sets.hxx"
#include "terminol/common/buffer.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/pattern.hxx"

#include <xkbcommon/xkbcommon.h>

class Terminal :
    protected VtStateMachine::I_Observer,
    protected Uncopyable
{
    struct CharSub {
        uint8_t   match;        // FIXME check for the 94/96 chars possible
        utf8::Seq replace;
    };

    static const uint16_t TAB_SIZE;
    static const CharSub  CS_US[];
    static const CharSub  CS_UK[];
    static const CharSub  CS_SPECIAL[];

public:
    class I_Observer {
    public:
        virtual void terminalCopy(const std::string & text, bool clipboard) throw () = 0;
        virtual void terminalPaste(bool clipboard) throw () = 0;
        virtual void terminalResizeFont(int delta) throw () = 0;
        virtual void terminalResetTitle() throw () = 0;
        virtual void terminalSetTitle(const std::string & title) throw () = 0;
        virtual void terminalResizeBuffer(uint16_t rows, uint16_t cols) throw () = 0;
        virtual bool terminalFixDamageBegin(bool internal) throw () = 0;
        virtual void terminalDrawRun(uint16_t        row,
                                     uint16_t        col,
                                     Style           style,
                                     const uint8_t * str,       // nul-terminated
                                     size_t          count) throw () = 0;
        virtual void terminalDrawCursor(uint16_t        row,
                                        uint16_t        col,
                                        Style           style,
                                        const uint8_t * str,    // nul-terminated, length 1
                                        bool            wrapNext) throw () = 0;
        virtual void terminalDrawSelection(uint16_t rowBegin,
                                           uint16_t colBegin,
                                           uint16_t rowEnd,
                                           uint16_t colEnd,
                                           bool     topless,
                                           bool     bottomless) throw () = 0;
        virtual void terminalDrawScrollbar(size_t   totalRows,
                                           size_t   historyOffset,
                                           uint16_t visibleRows) throw () = 0;
        virtual void terminalFixDamageEnd(bool     internal,
                                          uint16_t rowBegin,
                                          uint16_t rowEnd,
                                          uint16_t colBegin,
                                          uint16_t colEnd,
                                          bool     scrollbar) throw () = 0;
        virtual void terminalChildExited(int exitStatus) throw () = 0;

    protected:
        ~I_Observer() {}
    };

    //
    //
    //

    enum class Button    { LEFT, MIDDLE, RIGHT };
    enum class ScrollDir { UP, DOWN };

private:
    struct Cursor {
        Cursor() :
            cs(CS_US),
            G0(CS_US),
            G1(CS_US),
            //
            row(0),
            col(0),
            wrapNext(false),
            originMode(false),
            //
            style(Style::normal()) {}

        void reset() { *this = Cursor(); }

        const CharSub * cs;
        const CharSub * G0;
        const CharSub * G1;
        //
        uint16_t        row;
        uint16_t        col;
        bool            wrapNext;
        bool            originMode;
        //
        Style           style;
    };

    struct Damage {
        Damage() : rowBegin(0), rowEnd(0), colBegin(0), colEnd(0) {}

        void reset() { *this = Damage(); }

        uint16_t rowBegin;
        uint16_t rowEnd;
        uint16_t colBegin;
        uint16_t colEnd;
    };

    //
    //
    //

    I_Observer          & _observer;
    bool                  _dispatch;

    const Config        & _config;
    const KeyMap        & _keyMap;

    Buffer                _priBuffer;
    Buffer                _altBuffer;
    Buffer              * _buffer;

    ModeSet               _modes;
    std::vector<bool>     _tabs;

    Cursor                _cursor;
    Cursor                _savedCursor;

    Damage                _damage;

    bool                  _pressed;
    Button                _button;
    uint16_t              _pointerRow;
    uint16_t              _pointerCol;

    //

    I_Tty               & _tty;

    bool                  _dumpWrites;
    std::vector<uint8_t>  _writeBuffer;      // Spillover if the TTY would block.

    utf8::Machine         _utf8Machine;
    VtStateMachine        _vtMachine;

public:
    Terminal(I_Observer   & observer,
             const Config & config,
             uint16_t       rows,
             uint16_t       cols,
             const KeyMap & keyMap,
             I_Tty        & tty);
    virtual ~Terminal();

    // Geometry:

    uint16_t getRows() const { return _buffer->getRows(); }
    uint16_t getCols() const { return _buffer->getCols(); }

    // Events:

    void     resize(uint16_t rows, uint16_t cols);

    void     redraw(uint16_t rowBegin, uint16_t rowEnd,
                    uint16_t colBegin, uint16_t colEnd);

    void     keyPress(xkb_keysym_t keySym, uint8_t state);
    void     buttonPress(Button button, int count, uint8_t state,
                         bool within, uint16_t row, uint16_t col);
    void     buttonMotion(uint8_t state, bool within, uint16_t row, uint16_t col);
    void     buttonRelease(bool broken, uint8_t state);
    void     scrollWheel(ScrollDir dir, uint8_t state);

    void     paste(const uint8_t * data, size_t size);

    // I/O:

    void     read();
    bool     needsFlush() const;
    void     flush();

protected:
    enum class TabDir { FORWARD, BACKWARD };
    enum class Damager { TTY, EXPOSURE, SCROLL };

    bool      handleKeyBinding(xkb_keysym_t keySym, uint8_t state);

    void      moveCursorOriginMode(int32_t row, int32_t col);
    void      moveCursor(int32_t row, int32_t col);
    void      tabCursor(TabDir dir, uint16_t count);
    void      damageCursor();

    void      fixDamage(uint16_t rowBegin, uint16_t rowEnd,
                        uint16_t colBegin, uint16_t colEnd,
                        Damager damage);

    utf8::Seq translate(utf8::Seq seq, utf8::Length length) const;

    void      draw(uint16_t rowBegin, uint16_t rowEnd,
                   uint16_t colBegin, uint16_t colEnd,
                   Damager damage);

    void      write(const uint8_t * data, size_t size);

    void      resetAll();

    void      processRead(const uint8_t * data, size_t size);
    void      processChar(utf8::Seq seq, utf8::Length length);

    void      processAttributes(const std::vector<int32_t> & args);
    void      processModes(bool priv, bool set, const std::vector<int32_t> & args);

    // VtStateMachine::I_Observer overrides:

    void machineNormal(utf8::Seq seq, utf8::Length length) throw ();
    void machineControl(uint8_t c) throw ();
    void machineEscape(uint8_t c) throw ();
    void machineCsi(bool priv,
                    const std::vector<int32_t> & args,
                    uint8_t code) throw ();
    void machineDcs(const std::vector<uint8_t> & seq) throw ();
    void machineOsc(const std::vector<std::string> & args) throw ();
    void machineSpecial(uint8_t special, uint8_t code) throw ();
};

std::ostream & operator << (std::ostream & ost, Terminal::Button button);
std::ostream & operator << (std::ostream & ost, Terminal::ScrollDir dir);

#endif // COMMON__TERMINAL__HXX
