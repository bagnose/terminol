// vi:noai:sw=4

#ifndef COMMON__TERMINAL__HXX
#define COMMON__TERMINAL__HXX

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/vt_state_machine.hxx"
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
        virtual void terminalGetDisplay(std::string & display) throw () = 0;
        virtual void terminalCopy(const std::string & text, bool clipboard) throw () = 0;
        virtual void terminalPaste(bool clipboard) throw () = 0;
        virtual void terminalResizeFont(int delta) throw () = 0;
        virtual void terminalResetTitleAndIcon() throw () = 0;
        virtual void terminalSetWindowTitle(const std::string & str) throw () = 0;
        virtual void terminalSetIconName(const std::string & str) throw () = 0;
        virtual void terminalBeep() throw () = 0;
        virtual void terminalResizeBuffer(uint16_t rows, uint16_t cols) throw () = 0;
        virtual bool terminalFixDamageBegin(bool internal) throw () = 0;
        virtual void terminalDrawBg(Pos    pos,
                                    UColor color,
                                    size_t count) throw () = 0;
        virtual void terminalDrawFg(Pos             pos,
                                    UColor          color,
                                    AttrSet         attrs,
                                    const uint8_t * str,       // nul-terminated
                                    size_t          size,
                                    size_t          count) throw () = 0;
        virtual void terminalDrawCursor(Pos             pos,
                                        UColor          fg,
                                        UColor          bg,
                                        AttrSet         attrs,
                                        const uint8_t * str,    // nul-terminated, length 1
                                        size_t          size,
                                        bool            wrapNext,
                                        bool            focused) throw () = 0;
        virtual void terminalDrawSelection(Pos      begin,
                                           Pos      end,
                                           bool     topless,
                                           bool     bottomless) throw () = 0;
        virtual void terminalDrawScrollbar(size_t   totalRows,
                                           size_t   historyOffset,
                                           uint16_t visibleRows) throw () = 0;
        virtual void terminalFixDamageEnd(bool     internal,
                                          Pos      begin,
                                          Pos      end,
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
        enum class CharSet { G0, G1 };

        Cursor() :
            g0(CS_US),
            g1(CS_US),
            cs(CharSet::G0),
            //
            pos(),
            wrapNext(false),
            originMode(false),
            //
            style() {}

        void reset() { *this = Cursor(); }

        const CharSub *  getCurrentCS() const { return cs == CharSet::G0 ? g0 : g1; }

        const CharSub *  g0;
        const CharSub *  g1;
        CharSet          cs;
        //
        Pos              pos;
        bool             wrapNext;
        bool             originMode;
        //
        Style            style;
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

    Region                _damage;

    bool                  _pressed;
    Button                _button;
    Pos                   _pointerPos;
    bool                  _focused;

    //

    I_Tty               & _tty;

    bool                  _dumpWrites;
    std::vector<uint8_t>  _writeBuffer;      // Spillover if the TTY would block.

    utf8::Machine         _utf8Machine;
    VtStateMachine        _vtMachine;

public:
    Terminal(I_Observer   & observer,
             const Config & config,
             Deduper      & deduper,
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

    void     redraw(Pos begin, Pos end);

    void     keyPress(xkb_keysym_t keySym, ModifierSet modifiers);
    void     buttonPress(Button button, int count, ModifierSet modifiers,
                         bool within, Pos pos);
    void     buttonMotion(ModifierSet modifiers, bool within, Pos pos);
    void     buttonRelease(bool broken, ModifierSet modifiers);
    void     scrollWheel(ScrollDir dir, ModifierSet modifiers, bool within, Pos pos);

    void     paste(const uint8_t * data, size_t size);

    void     clearSelection();

    void     focusChange(bool focused);

    // I/O:

    void     read();
    bool     needsFlush() const;
    void     flush();

protected:
    enum class TabDir { FORWARD, BACKWARD };
    enum class Damager { TTY, EXPOSURE, SCROLL };

    bool     handleKeyBinding(xkb_keysym_t keySym, ModifierSet modifiers);

    void     moveCursorOriginMode(Pos pos);
    void     moveCursor(Pos pos);
    void     tabCursor(TabDir dir, uint16_t count);
    void     damageCursor();

    void     fixDamage(Pos begin, Pos end, Damager damager);

    bool     translate(uint8_t ascii, utf8::Seq & seq) const;

    void     draw(Pos begin, Pos end, Damager damage);
    void     drawRowBg(uint16_t row, uint16_t colBegin, uint16_t colEnd);
    void     drawRowFg(std::vector<uint8_t> & run,
                       uint16_t row, uint16_t colBegin, uint16_t colEnd);
    void     drawCursor(std::vector<uint8_t> & run);
    void     drawSelection();

    void     write(const uint8_t * data, size_t size);
    void     echo(const uint8_t * data, size_t size);

    void     sendMouseButton(int num, ModifierSet modifiers, Pos pos);

    void     resetAll();

    void     processRead(const uint8_t * data, size_t size);
    void     processChar(utf8::Seq seq, utf8::Length length);

    void     processAttributes(const std::vector<int32_t> & args);
    void     processModes(uint8_t priv, bool set, const std::vector<int32_t> & args);

    // VtStateMachine::I_Observer implementation:

    void     machineNormal(utf8::Seq seq, utf8::Length length) throw ();
    void     machineControl(uint8_t c) throw ();
    void     machineEscape(uint8_t c) throw ();
    void     machineCsi(uint8_t priv,
                        const std::vector<int32_t> & args,
                        const std::vector<uint8_t> & inters,
                        uint8_t code) throw ();
    void     machineDcs(const std::vector<uint8_t> & seq) throw ();
    void     machineOsc(const std::vector<std::string> & args) throw ();
    void     machineSpecial(uint8_t special, uint8_t code) throw ();
};

std::ostream & operator << (std::ostream & ost, Terminal::Button button);
std::ostream & operator << (std::ostream & ost, Terminal::ScrollDir dir);

#endif // COMMON__TERMINAL__HXX
