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
        utf8::Seq match;
        utf8::Seq replace;
    };

    static const CharSub CS_US[];
    static const CharSub CS_UK[];
    static const CharSub CS_SPECIAL[];

public:
    class I_Observer {
    public:
        virtual void terminalResetTitle() throw () = 0;
        virtual void terminalSetTitle(const std::string & title) throw () = 0;
        virtual bool terminalBeginFixDamage(bool internal) throw () = 0;
        virtual void terminalDrawRun(uint16_t       row,
                                     uint16_t       col,
                                     uint8_t        fg,
                                     uint8_t        bg,
                                     AttributeSet   attrs,
                                     const char   * str,       // nul-terminated
                                     size_t         count) throw () = 0;
        virtual void terminalDrawCursor(uint16_t       row,
                                        uint16_t       col,
                                        uint8_t        fg,
                                        uint8_t        bg,
                                        AttributeSet   attrs,
                                        const char   * str,
                                        bool           special) throw () = 0;
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

    const CharSub       * _G0;
    const CharSub       * _G1;
    const CharSub       * _CS;
    uint16_t              _cursorRow;
    uint16_t              _cursorCol;
    uint8_t               _bg;
    uint8_t               _fg;
    AttributeSet          _attrs;
    bool                  _originMode;

    ModeSet               _modes;
    std::vector<bool>     _tabs;

    const CharSub       * _savedG0;
    const CharSub       * _savedG1;
    const CharSub       * _savedCS;
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
    std::vector<char>     _writeBuffer;      // Spillover if the TTY would block.

    State                 _state;
    State                 _outerState;
    utf8::Machine         _utf8Machine;
    std::vector<char>     _escSeq;

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
    void      damageCursor();

    void      fixDamage(uint16_t rowBegin, uint16_t rowEnd,
                        uint16_t colBegin, uint16_t colEnd,
                        bool internal);

    utf8::Seq translate(utf8::Seq seq) const;

    void      draw(uint16_t rowBegin, uint16_t rowEnd,
                   uint16_t colBegin, uint16_t colEnd,
                   bool internal);

    void      write(const char * data, size_t size);

    void      resetAll();

    void      processRead(const char * data, size_t size);
    void      processChar(utf8::Seq seq, utf8::Length length);
    void      processControl(char c);
    void      processNormal(utf8::Seq seq);
    void      processEscape(char c);
    void      processCsi(const std::vector<char> & seq);
    void      processDcs(const std::vector<char> & seq);
    void      processOsc(const std::vector<char> & seq);
    void      processSpecial(const std::vector<char> & seq);
    void      processAttributes(const std::vector<int32_t> & args);
    void      processModes(bool priv, bool set, const std::vector<int32_t> & args);
};

#endif // COMMON__TERMINAL__HXX
