// vi:noai:sw=4

#ifndef COMMON__TERMINAL__HXX
#define COMMON__TERMINAL__HXX

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/bit_sets.hxx"
#include "terminol/common/buffer.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/common/support.hxx"

#include <xkbcommon/xkbcommon.h>

class Terminal : protected Uncopyable {
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
        virtual void terminalResetTitle() throw () = 0;
        virtual void terminalSetTitle(const std::string & title) throw () = 0;
        virtual void terminalResize(uint16_t rows, uint16_t cols) throw () = 0;
        virtual bool terminalBeginFixDamage(bool internal) throw () = 0;
        virtual void terminalDrawRun(uint16_t        row,
                                     uint16_t        col,
                                     uint8_t         fg,
                                     uint8_t         bg,
                                     AttributeSet    attrs,
                                     const uint8_t * str,       // nul-terminated
                                     size_t          count) throw () = 0;
        virtual void terminalDrawCursor(uint16_t        row,
                                        uint16_t        col,
                                        uint8_t         fg,
                                        uint8_t         bg,
                                        AttributeSet    attrs,
                                        const uint8_t * str,
                                        bool            special) throw () = 0;
        virtual void terminalEndFixDamage(bool     internal,
                                          uint16_t rowBegin,
                                          uint16_t rowEnd,
                                          uint16_t colBegin,
                                          uint16_t colEnd) throw () = 0;
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
    Buffer                _priBuffer;
    Buffer                _altBuffer;
    Buffer              * _buffer;

    bool                  _otherCharSet;
    const CharSub       * _G0;
    const CharSub       * _G1;
    uint16_t              _cursorRow;
    uint16_t              _cursorCol;
    uint8_t               _bg;
    uint8_t               _fg;
    AttributeSet          _attrs;
    bool                  _originMode;

    ModeSet               _modes;
    std::vector<bool>     _tabs;

    bool                  _savedOtherCharSet;
    const CharSub       * _savedG0;
    const CharSub       * _savedG1;
    uint16_t              _savedCursorRow;
    uint16_t              _savedCursorCol;
    uint8_t               _savedFg;
    uint8_t               _savedBg;
    AttributeSet          _savedAttrs;
    bool                  _savedOriginMode;

    uint16_t              _damageRowBegin;
    uint16_t              _damageRowEnd;
    uint16_t              _damageColBegin;
    uint16_t              _damageColEnd;

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
    std::vector<uint8_t>  _writeBuffer;      // Spillover if the TTY would block.

    State                 _state;
    State                 _outerState;
    utf8::Machine         _utf8Machine;
    std::vector<uint8_t>  _escSeq;

    bool                  _trace;
    bool                  _sync;

public:
    Terminal(I_Observer   & observer,
             const KeyMap & keyMap,
             I_Tty        & tty,
             uint16_t       rows,
             uint16_t       cols,
             bool           trace,
             bool           sync);
    virtual ~Terminal();

    uint16_t getRows() const { return _buffer->getRows(); }
    uint16_t getCols() const { return _buffer->getCols(); }

    void     resize(uint16_t rows, uint16_t cols);

    void     redraw(uint16_t rowBegin, uint16_t rowEnd,
                    uint16_t colBegin, uint16_t colEnd);

    void     keyPress(xkb_keysym_t keySym, uint8_t state);
    void     read();
    bool     needsFlush() const;
    void     flush();

protected:
    enum TabDir { FORWARD, BACKWARD };

    void      moveCursor(int32_t row, int32_t col);
    void      tabCursor(TabDir dir, uint16_t count);

    void      damageCursor();

    void      fixDamage(uint16_t rowBegin, uint16_t rowEnd,
                        uint16_t colBegin, uint16_t colEnd,
                        bool internal);

    utf8::Seq translate(utf8::Seq seq) const;

    void      draw(uint16_t rowBegin, uint16_t rowEnd,
                   uint16_t colBegin, uint16_t colEnd,
                   bool internal);

    void      write(const uint8_t * data, size_t size);

    void      resetAll();

    void      processRead(const uint8_t * data, size_t size);
    void      processChar(utf8::Seq seq, utf8::Length length);
    void      processControl(uint8_t c);
    void      processNormal(utf8::Seq seq);
    void      processEscape(uint8_t c);
    void      processCsi(const std::vector<uint8_t> & seq);
    void      processDcs(const std::vector<uint8_t> & seq);
    void      processOsc(const std::vector<uint8_t> & seq);
    void      processSpecial(const std::vector<uint8_t> & seq);
    void      processAttributes(const std::vector<int32_t> & args);
    void      processModes(bool priv, bool set, const std::vector<int32_t> & args);
};

#endif // COMMON__TERMINAL__HXX
