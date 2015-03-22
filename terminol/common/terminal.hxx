// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__TERMINAL__HXX
#define COMMON__TERMINAL__HXX

#include "terminol/common/tty.hxx"
#include "terminol/common/vt_state_machine.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/bit_sets.hxx"
#include "terminol/common/buffer.hxx"
#include "terminol/common/deduper_interface.hxx"
#include "terminol/support/selector.hxx"
#include "terminol/support/pattern.hxx"

#include <xkbcommon/xkbcommon.h>

class Terminal :
    protected VtStateMachine::I_Observer,
    protected Tty::I_Observer,
    protected Buffer::I_Renderer,
    protected Uncopyable
{
    static const CharSub CS_US;
    static const CharSub CS_UK;
    static const CharSub CS_SPECIAL;

public:
    enum class Selection { PRIMARY, CLIPBOARD };

    class I_Observer {
    public:
        virtual const std::string & terminalGetDisplayName() const throw () = 0;
        virtual void terminalCopy(const std::string & text, Selection selection) throw () = 0;
        virtual void terminalPaste(Selection selection) throw () = 0;
        virtual void terminalResizeLocalFont(int delta) throw () = 0;
        virtual void terminalResizeGlobalFont(int delta) throw () = 0;
        virtual void terminalResetTitleAndIcon() throw () = 0;
        virtual void terminalSetWindowTitle(const std::string & str, bool transient) throw () = 0;
        virtual void terminalSetIconName(const std::string & str) throw () = 0;
        virtual void terminalBell() throw () = 0;
        virtual void terminalResizeBuffer(int16_t rows, int16_t cols) throw () = 0;
        virtual bool terminalFixDamageBegin() throw () = 0;
        virtual void terminalDrawBg(Pos     pos,
                                    int16_t count,
                                    UColor  color) throw () = 0;
        virtual void terminalDrawFg(Pos             pos,
                                    int16_t         count,
                                    UColor          color,
                                    AttrSet         attrs,
                                    const uint8_t * str,       // nul-terminated
                                    size_t          size) throw () = 0;
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
        virtual void terminalReaped(int status) throw () = 0;

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

    utf8::Machine         _utf8Machine;
    VtStateMachine        _vtMachine;
    Tty                   _tty;

public:
    Terminal(I_Observer         & observer,
             const Config       & config,
             I_Selector         & selector,
             I_Deduper          & deduper,
             int16_t              rows,
             int16_t              cols,
             const std::string  & windowId,
             const Tty::Command & command) throw (Tty::Error);
    virtual ~Terminal();

    // Geometry:

    int16_t getRows() const { return _buffer->getRows(); }
    int16_t getCols() const { return _buffer->getCols(); }

    // Events:

    void     resize(int16_t rows, int16_t cols);

    void     redraw();

    bool     keyPress(xkb_keysym_t keySym, ModifierSet modifiers);
    void     buttonPress(Button button, int count, ModifierSet modifiers,
                         bool within, Pos pos, Hand hand);
    void     pointerMotion(ModifierSet modifiers, bool within, Pos pos, Hand hand);
    void     buttonRelease(bool broken, ModifierSet modifiers);
    void     scrollWheel(ScrollDir dir, ModifierSet modifiers, bool within, Pos pos);

    void     paste(const uint8_t * data, size_t size);

    void     tryReap();
    void     killReap();
    void     clearSelection();

    void     focusChange(bool focused);

    bool     hasSubprocess() const;

protected:
    enum class Trigger { TTY, FOCUS, CLIENT, OTHER };

    bool     handleKeyBinding(xkb_keysym_t keySym, ModifierSet modifiers);

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

    static const CharSub * lookupCharSub(uint8_t code);

    // VtStateMachine::I_Observer implementation:

    void     machineNormal(utf8::Seq seq, utf8::Length length) throw () override;
    void     machineControl(uint8_t control) throw () override;
    void     machineSimpleEsc(const SimpleEsc & esc) throw () override;
    void     machineCsiEsc(const CsiEsc & esc) throw () override;
    void     machineDcsEsc(const DcsEsc & esc) throw () override;
    void     machineOscEsc(const OscEsc & esc) throw () override;

    // Tty::I_Observer implementation:

    void     ttyData(const uint8_t * data, size_t size) throw () override;
    void     ttySync() throw () override;
    void     ttyReaped(int status) throw () override;

    // Buffer::I_Renderer implementation:

    void     bufferDrawBg(Pos     pos,
                          int16_t count,
                          UColor  color) throw () override;
    void     bufferDrawFg(Pos             pos,
                          int16_t         count,
                          UColor          color,
                          AttrSet         attrs,
                          const uint8_t * str,
                          size_t          size) throw () override;
    void     bufferDrawCursor(Pos             pos,
                              UColor          fg,
                              UColor          bg,
                              AttrSet         attrs,
                              const uint8_t * str,
                              size_t          size,
                              bool            wrapNext) throw () override;
};

std::ostream & operator << (std::ostream & ost, Terminal::Button button);
std::ostream & operator << (std::ostream & ost, Terminal::ScrollDir dir);

#endif // COMMON__TERMINAL__HXX
