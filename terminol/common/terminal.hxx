// vi:noai:sw=4

#ifndef COMMON__TERMINAL__HXX
#define COMMON__TERMINAL__HXX

#include "terminol/common/tty_interface.hxx"
#include "terminol/common/vt_state_machine.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/bit_sets.hxx"
#include "terminol/common/buffer.hxx"
#include "terminol/common/deduper_interface.hxx"
#include "terminol/support/pattern.hxx"

#include <xkbcommon/xkbcommon.h>

class Terminal :
    protected VtStateMachine::I_Observer,
    protected Uncopyable
{
    static const CharSub CS_US;
    static const CharSub CS_UK;
    static const CharSub CS_SPECIAL;

public:
    class I_Observer {
    public:
        virtual void terminalGetDisplay(std::string & display) throw () = 0;
        virtual void terminalCopy(const std::string & text, bool clipboard) throw () = 0;
        virtual void terminalPaste(bool clipboard) throw () = 0;
        virtual void terminalResizeLocalFont(int delta) throw () = 0;
        virtual void terminalResizeGlobalFont(int delta) throw () = 0;
        virtual void terminalResetTitleAndIcon() throw () = 0;
        virtual void terminalSetWindowTitle(const std::string & str) throw () = 0;
        virtual void terminalSetIconName(const std::string & str) throw () = 0;
        virtual void terminalBeep() throw () = 0;
        virtual void terminalResizeBuffer(int16_t rows, int16_t cols) throw () = 0;
        virtual bool terminalFixDamageBegin() throw () = 0;
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
                                        const uint8_t * str,    // nul-terminated, count 1
                                        size_t          size,
                                        bool            wrapNext,
                                        bool            focused) throw () = 0;
        virtual void terminalDrawScrollbar(size_t  totalRows,
                                           size_t  historyOffset,
                                           int16_t visibleRows) throw () = 0;
        virtual void terminalFixDamageEnd(const Region & damage,
                                          bool           scrollbar) throw () = 0;
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
    I_Observer          & _observer;
    bool                  _dispatch;

    const Config        & _config;
    const I_Deduper     & _deduper;

    Buffer                _priBuffer;
    Buffer                _altBuffer;
    Buffer              * _buffer;

    ModeSet               _modes;

    enum class Press { NONE, SELECT, REPORT };

    Press                 _press;
    Button                _button;
    Pos                   _pointerPos;
    bool                  _focused;

    utf8::Seq             _lastSeq;

    //

    I_Tty               & _tty;

    bool                  _dumpWrites;
    std::vector<uint8_t>  _writeBuffer;      // Spillover if the TTY would block.

    utf8::Machine         _utf8Machine;
    VtStateMachine        _vtMachine;

public:
    Terminal(I_Observer   & observer,
             const Config & config,
             I_Deduper    & deduper,
             int16_t        rows,
             int16_t        cols,
             I_Tty        & tty);
    virtual ~Terminal();

    // Geometry:

    int16_t getRows() const { return _buffer->getRows(); }
    int16_t getCols() const { return _buffer->getCols(); }

    // Events:

    void     resize(int16_t rows, int16_t cols);

    void     redraw();

    bool     keyPress(xkb_keysym_t keySym, ModifierSet modifiers);
    void     buttonPress(Button button, int count, ModifierSet modifiers,
                         bool within, HPos hpos);
    void     pointerMotion(ModifierSet modifiers, bool within, HPos hpos);
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
    enum class Trigger { TTY, SCROLL, FOCUS, CLIENT };

    bool     handleKeyBinding(xkb_keysym_t keySym, ModifierSet modifiers);

    void     moveCursor(Pos pos, bool considerOriginMode = false);

    void     fixDamage(Trigger trigger);

    void     draw(Trigger trigger, Region & damage, bool & scrollbar);

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
    void     machineControl(uint8_t control) throw ();
    void     machineEscape(uint8_t code) throw ();
    void     machineCsi(uint8_t priv,
                        const std::vector<int32_t> & args,
                        const std::vector<uint8_t> & inters,
                        uint8_t code) throw ();
    void     machineDcs(const std::vector<uint8_t> & seq) throw ();
    void     machineOsc(const std::vector<std::string> & args) throw ();
    void     machineSpecial(const std::vector<uint8_t> & inter,
                            uint8_t code) throw ();
};

std::ostream & operator << (std::ostream & ost, Terminal::Button button);
std::ostream & operator << (std::ostream & ost, Terminal::ScrollDir dir);

#endif // COMMON__TERMINAL__HXX
