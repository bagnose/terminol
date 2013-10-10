// vi:noai:sw=4
// Copyright © 2013 David Bryant

#include "terminol/common/terminal.hxx"
#include "terminol/common/key_map.hxx"
#include "terminol/support/conv.hxx"
#include "terminol/support/escape.hxx"

#include <algorithm>
#include <numeric>

namespace {

int32_t nthArg(const std::vector<int32_t> & args, size_t n, int32_t fallback = 0) {
    return n < args.size() ? args[n] : fallback;
}

// Same as nth arg, but use fallback if arg is zero.
int32_t nthArgNonZero(const std::vector<int32_t> & args, size_t n, int32_t fallback) {
    auto arg = nthArg(args, n, fallback);
    return arg != 0 ? arg : fallback;
}

const utf8::Seq UK_SEQS[] = {
    { 0xC2, 0xA3 }        // POUND: £
};

const utf8::Seq SPECIAL_SEQS[] = {
    { 0xE2, 0x99, 0xA6 }, // diamond: ♦
    { 0xE2, 0x96, 0x92 }, // 50% cell: ▒
    { 0xE2, 0x90, 0x89 }, // HT: ␉
    { 0xE2, 0x90, 0x8C }, // FF: ␌
    { 0xE2, 0x90, 0x8D }, // CR: ␍
    { 0xE2, 0x90, 0x8A }, // LF: ␊
    { 0xC2, 0xB0       }, // Degree: °
    { 0xC2, 0xB1       }, // Plus/Minus: ±
    { 0xE2, 0x90, 0xA4 }, // NL: ␤
    { 0xE2, 0x90, 0x8B }, // VT: ␋
    { 0xE2, 0x94, 0x98 }, // CN_RB: ┘
    { 0xE2, 0x94, 0x90 }, // CN_RT: ┐
    { 0xE2, 0x94, 0x8C }, // CN_LT: ┌
    { 0xE2, 0x94, 0x94 }, // CN_LB: └
    { 0xE2, 0x94, 0xBC }, // CROSS: ┼
    { 0xE2, 0x8E, 0xBA }, // Horiz. Scan Line 1: ⎺
    { 0xE2, 0x8E, 0xBB }, // Horiz. Scan Line 3: ⎻
    { 0xE2, 0x94, 0x80 }, // Horiz. Scan Line 5: ─
    { 0xE2, 0x8E, 0xBC }, // Horiz. Scan Line 7: ⎼
    { 0xE2, 0x8E, 0xBD }, // Horiz. Scan Line 9: ⎽
    { 0xE2, 0x94, 0x9C }, // TR: ├
    { 0xE2, 0x94, 0xA4 }, // TL: ┤
    { 0xE2, 0x94, 0xB4 }, // TU: ┴
    { 0xE2, 0x94, 0xAC }, // TD: ┬
    { 0xE2, 0x94, 0x82 }, // V: │
    { 0xE2, 0x89, 0xA4 }, // LE: ≤
    { 0xE2, 0x89, 0xA5 }, // GE: ≥
    { 0xCF, 0x80       }, // PI: π
    { 0xE2, 0x89, 0xA0 }, // NEQ: ≠
    { 0xC2, 0xA3       }, // POUND: £
    { 0xE2, 0x8B, 0x85 }  // DOT: ⋅
};

} // namespace {anonymous}

//
//
//

static_assert(STATIC_ARRAY_SIZE(UK_SEQS)      == 1, "Incorrect size: UK_SEQS.");
static_assert(STATIC_ARRAY_SIZE(SPECIAL_SEQS) == 31, "Incorrect size: SPECIAL_SEQS.");

const CharSub Terminal::CS_US;
const CharSub Terminal::CS_UK(UK_SEQS, 35, 1);
const CharSub Terminal::CS_SPECIAL(SPECIAL_SEQS, 96, 31, true);

Terminal::Terminal(I_Observer         & observer,
                   const Config       & config,
                   I_Selector         & selector,
                   I_Deduper          & deduper,
                   int16_t              rows,
                   int16_t              cols,
                   const std::string  & windowId,
                   const Tty::Command & command) throw (Tty::Error) :
    _observer(observer),
    //
    _config(config),
    _deduper(deduper),
    //
    _priBuffer(_config, deduper, rows, cols,
               _config.unlimitedScrollBack ?
               std::numeric_limits<int32_t>::max() :
               _config.scrollBackHistory,
               CharSubArray(&CS_US, &CS_SPECIAL, &CS_US, &CS_US)),
    _altBuffer(_config, deduper, rows, cols, 0,
               CharSubArray(&CS_US, &CS_SPECIAL, &CS_US, &CS_US)),
    _buffer(&_priBuffer),
    //
    _modes(),
    //
    _press(Press::NONE),
    _button(Button::LEFT),
    _pointerPos(),
    _focused(true),
    _lastSeq(),
    //
    _utf8Machine(),
    _vtMachine(*this, _config),
    _tty(*this, selector, config, rows, cols, windowId, command)
{
    _modes.set(Mode::AUTO_WRAP);
    _modes.set(Mode::SHOW_CURSOR);
    _modes.set(Mode::AUTO_REPEAT);
    _modes.set(Mode::ALT_SENDS_ESC);
}

Terminal::~Terminal() {}

void Terminal::resize(int16_t rows, int16_t cols) {
    // Special exception, resizes can occur during dispatch to support
    // font size changes.

    ASSERT(rows > 0 && cols > 0, "Rows or cols not positive.");

    _priBuffer.resizeReflow(rows, cols);
    _altBuffer.resizeClip(rows, cols);
    _tty.resize(rows, cols);
}

void Terminal::redraw() {
    Region damage;
    bool   scrollbar;
    draw(Trigger::CLIENT, damage, scrollbar);
}

bool Terminal::keyPress(xkb_keysym_t keySym, ModifierSet modifiers) {
    if (!handleKeyBinding(keySym, modifiers) && xkb::isPotent(keySym)) {
        if (_config.scrollOnTtyKeyPress && _buffer->scrollBottomHistory()) {
            fixDamage(Trigger::OTHER);
        }

        std::vector<uint8_t> input;
        if (xkb::composeInput(keySym, modifiers,
                              _modes.get(Mode::APPKEYPAD),
                              _modes.get(Mode::APPCURSOR),
                              _modes.get(Mode::CR_ON_LF),
                              _modes.get(Mode::DELETE_SENDS_DEL),
                              _modes.get(Mode::ALT_SENDS_ESC),
                              input)) {

            if (input.size() == 1 && _modes.get(Mode::META_8BIT) &&
                modifiers.get(Modifier::ALT))
            {
                PRINT("8-bit conversion");
                utf8::CodePoint cp = input[0] | (1 << 7);
                uint8_t seq[utf8::Length::LMAX];
                utf8::Length l = utf8::encode(cp, seq);
                input.resize(l);
                std::copy(seq, seq + l, input.begin());
            }

            write(&input.front(), input.size());
            if (_modes.get(Mode::ECHO)) { echo(&input.front(), input.size()); }
        }

        return true;
    }
    else {
        return false;
    }
}

void Terminal::buttonPress(Button button, int count, ModifierSet modifiers,
                           bool UNUSED(within), Pos pos, Hand hand) {
    ASSERT(_press == Press::NONE, "Received button press but already got one.");

    Pos adjPos(pos.row, pos.col + (hand == Hand::RIGHT ? 1 : 0));

    if (_modes.get(Mode::MOUSE_PRESS_RELEASE)) {
        sendMouseButton(static_cast<int>(button), modifiers, pos);
        if (_modes.get(Mode::MOUSE_SELECT)) {
            goto select;
        }
        else {
            _press = Press::REPORT;
        }
    }
    else {
select:
        if (button == Button::LEFT) {
            if (count == 1) {
                _buffer->markSelection(adjPos);
            }
            else {
                _buffer->expandSelection(pos, count);
            }

            fixDamage(Trigger::OTHER);
        }
        else if (button == Button::MIDDLE) {
            _observer.terminalPaste(Terminal::Selection::PRIMARY);
        }
        else if (button == Button::RIGHT) {
            _buffer->delimitSelection(adjPos, true);
            fixDamage(Trigger::OTHER);
        }
        _press = Press::SELECT;
    }

    _button     = button;
    _pointerPos = pos;

    ASSERT(_press != Press::NONE, "Button press should be recorded.");
}

void Terminal::pointerMotion(ModifierSet modifiers, bool within, Pos pos, Hand hand) {
    if ((_press == Press::REPORT && _modes.get(Mode::MOUSE_DRAG)) ||
        (_press == Press::NONE   && _modes.get(Mode::MOUSE_MOTION)))
    {
        if (within) {
            int num = static_cast<int>(_button) + 32;

            if (modifiers.get(Modifier::SHIFT))   { num +=  1; }
            if (modifiers.get(Modifier::ALT))     { num +=  2; }
            if (modifiers.get(Modifier::CONTROL)) { num +=  4; }

            std::ostringstream ost;
            if (_modes.get(Mode::MOUSE_FORMAT_SGR)) {
                ost << ESC << "[<"
                    << num << ';' << pos.col + 1 << ';' << pos.row + 1
                    << 'M';
            }
            else if (pos.row < 223 && pos.col < 223) {
                ost << ESC << "[M"
                    << static_cast<char>(32 + num)
                    << static_cast<char>(32 + pos.col + 1)
                    << static_cast<char>(32 + pos.row + 1);
            }
            else {
                // Couldn't deliver it
            }

            const auto & str = ost.str();

            if (!str.empty()) {
                write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
            }
        }
    }
    else if (_press == Press::SELECT) {
        if (_button == Button::LEFT || _button == Button::RIGHT) {
            Pos adjPos(pos.row, pos.col + (hand == Hand::RIGHT ? 1 : 0));
            _buffer->delimitSelection(adjPos, false);
            fixDamage(Trigger::OTHER);
        }
    }

    _pointerPos = pos;
}

void Terminal::buttonRelease(bool UNUSED(broken), ModifierSet modifiers) {
    ASSERT(_press != Press::NONE, "Received button release but have no press.");

    if (_press == Press::SELECT) {
        std::string text;
        if (_buffer->getSelectedText(text)) {
            _observer.terminalCopy(text, Selection::PRIMARY);
        }

        if (_modes.get(Mode::MOUSE_SELECT) && _modes.get(Mode::MOUSE_PRESS_RELEASE)) {
            goto report;
        }
    }
    else if (_press == Press::REPORT) {
        if (_modes.get(Mode::MOUSE_PRESS_RELEASE)) {
report:
            auto num = _modes.get(Mode::MOUSE_FORMAT_SGR) ? static_cast<int>(_button) + 32 : 3;
            auto pos = _pointerPos;

            if (modifiers.get(Modifier::SHIFT))   { num +=  4; }
            if (modifiers.get(Modifier::ALT))     { num +=  8; }
            if (modifiers.get(Modifier::CONTROL)) { num += 16; }

            std::ostringstream ost;
            if (_modes.get(Mode::MOUSE_FORMAT_SGR)) {
                ost << ESC << "[<"
                    << num << ';' << pos.col + 1 << ';' << pos.row + 1
                    << 'm';
            }
            else if (pos.row < 223 && pos.col < 223) {
                ost << ESC << "[M"
                    << static_cast<char>(32 + num)
                    << static_cast<char>(32 + pos.col + 1)
                    << static_cast<char>(32 + pos.row + 1);
            }
            else {
                // Couldn't deliver it
            }

            const auto & str = ost.str();

            if (!str.empty()) {
                write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
            }
        }
    }
    else {
        FATAL("Unreachable.");
    }

    _press = Press::NONE;
}

void Terminal::scrollWheel(ScrollDir dir, ModifierSet modifiers, bool UNUSED(within), Pos pos) {
    if (_modes.get(Mode::MOUSE_PRESS_RELEASE)) {
        sendMouseButton(dir == ScrollDir::UP ? 3 : 4, modifiers, pos);
    }
    else {
        int16_t rows =
            modifiers.get(Modifier::SHIFT) ?
            1 :
            std::max(1, _buffer->getRows() / 4);

        switch (dir) {
            case ScrollDir::UP:
                if (_buffer->scrollUpHistory(rows)) {
                    fixDamage(Trigger::OTHER);
                }
                break;
            case ScrollDir::DOWN:
                if (_buffer->scrollDownHistory(rows)) {
                    fixDamage(Trigger::OTHER);
                }
                break;
        }
    }
}

void Terminal::paste(const uint8_t * data, size_t size) {
    if (_config.scrollOnPaste && _buffer->scrollBottomHistory()) {
        fixDamage(Trigger::OTHER);
    }

    if (_modes.get(Mode::BRACKETED_PASTE)) {
        write(reinterpret_cast<const uint8_t *>("\x1B[200~"), 6);
    }

    write(data, size);

    if (_modes.get(Mode::BRACKETED_PASTE)) {
        write(reinterpret_cast<const uint8_t *>("\x1B[201~"), 6);
    }
}

void Terminal::tryReap() {
    _tty.tryReap();
}

void Terminal::killReap() {
    _tty.killReap();
}

void Terminal::clearSelection() {
    _buffer->clearSelection();
    fixDamage(Trigger::OTHER);
}

void Terminal::focusChange(bool focused) {
    if (_focused != focused) {
        _focused = focused;

        if (_modes.get(Mode::FOCUS)) {
            if (focused) {
                write(reinterpret_cast<const uint8_t *>("\x1B[I"), 3);
            }
            else {
                write(reinterpret_cast<const uint8_t *>("\x1B[O"), 3);
            }
        }

        if (_modes.get(Mode::SHOW_CURSOR)) {
            fixDamage(Trigger::FOCUS);
        }
    }
}

bool Terminal::hasSubprocess() const {
    return _tty.hasSubprocess();
}

bool Terminal::handleKeyBinding(xkb_keysym_t keySym, ModifierSet modifiers) {
    // Unset the modifiers that don't count when matching.
    modifiers.unset(Modifier::NUM_LOCK);
    modifiers.unset(Modifier::SHIFT_LOCK);
    modifiers.unset(Modifier::CAPS_LOCK);
    modifiers.unset(Modifier::MODE_SWITCH);

    auto iter = _config.bindings.find(KeyCombo(keySym, modifiers));

    if (iter != _config.bindings.end()) {
        auto action = iter->second;

        switch (action) {
            case Action::WINDOW_NARROWER:
                _observer.terminalResizeBuffer(getRows(), std::max(1, getCols() - 1));
                return true;
            case Action::WINDOW_WIDER:
                _observer.terminalResizeBuffer(getRows(), getCols() + 1);
                return true;
            case Action::WINDOW_SHORTER:
                _observer.terminalResizeBuffer(std::max(1, getRows() - 1), getCols());
                return true;
            case Action::WINDOW_TALLER:
                _observer.terminalResizeBuffer(getRows() + 1, getCols());
                return true;
            case Action::LOCAL_FONT_RESET:
                _observer.terminalResizeLocalFont(0);
                return true;
            case Action::LOCAL_FONT_BIGGER:
                _observer.terminalResizeLocalFont(1);
                return true;
            case Action::LOCAL_FONT_SMALLER:
                _observer.terminalResizeLocalFont(-1);
                return true;
            case Action::GLOBAL_FONT_RESET:
                _observer.terminalResizeGlobalFont(0);
                return true;
            case Action::GLOBAL_FONT_BIGGER:
                _observer.terminalResizeGlobalFont(1);
                return true;
            case Action::GLOBAL_FONT_SMALLER:
                _observer.terminalResizeGlobalFont(-1);
                return true;
            case Action::COPY_TO_CLIPBOARD: {
                std::string text;
                if (_buffer->getSelectedText(text)) {
                    _observer.terminalCopy(text, Selection::CLIPBOARD);
                }
                return true;
            }
            case Action::PASTE_FROM_CLIPBOARD:
                _observer.terminalPaste(Selection::CLIPBOARD);
                return true;
            case Action::SCROLL_UP_LINE:
                if (_buffer->scrollUpHistory(1)) {
                    fixDamage(Trigger::OTHER);
                }
                return true;
            case Action::SCROLL_DOWN_LINE:
                if (_buffer->scrollDownHistory(1)) {
                    fixDamage(Trigger::OTHER);
                }
                return true;
            case Action::SCROLL_UP_PAGE:
                if (_buffer->scrollUpHistory(_buffer->getRows())) {
                    fixDamage(Trigger::OTHER);
                }
                return true;
            case Action::SCROLL_DOWN_PAGE:
                if (_buffer->scrollDownHistory(_buffer->getRows())) {
                    fixDamage(Trigger::OTHER);
                }
                return true;
            case Action::SCROLL_TOP:
                if (_buffer->scrollTopHistory()) {
                    fixDamage(Trigger::OTHER);
                }
                return true;
            case Action::SCROLL_BOTTOM:
                if (_buffer->scrollBottomHistory()) {
                    fixDamage(Trigger::OTHER);
                }
                return true;
            case Action::CLEAR_HISTORY:
                _priBuffer.clearHistory();
                fixDamage(Trigger::OTHER);
                return true;
            case Action::SEARCH:
                if (_buffer->isSearching()) {
                    _tty.resume();
                    _buffer->endSearch();
                }
                else {
                    _tty.suspend();
                    _buffer->beginSearch("da");
                }
                fixDamage(Trigger::CLIENT); // kludgy
                return true;
            case Action::DEBUG_GLOBAL_TAGS:
                _deduper.dump(std::cerr);
                return true;
            case Action::DEBUG_LOCAL_TAGS:
                _buffer->dumpTags(std::cerr);
                return true;
            case Action::DEBUG_HISTORY:
                _buffer->dumpHistory(std::cerr);
                return true;
            case Action::DEBUG_ACTIVE:
                _buffer->dumpActive(std::cerr);
                return true;
            case Action::DEBUG_MODES: {
                std::ostringstream ost;
                ost << _modes;
                _observer.terminalSetWindowTitle(ost.str(), true);
                return true;
            }
            case Action::DEBUG_SELECTION:
                _buffer->dumpSelection(std::cerr);
                return true;
            case Action::DEBUG_STATS: {
                size_t uniqueBytes, totalBytes;
                _deduper.getByteStats(uniqueBytes, totalBytes);

                std::ostringstream ost;
                ost << "line-data="   << humanSize(uniqueBytes) << " "
                    << "(non-dedupe=" << humanSize(totalBytes) << ")";
                _observer.terminalSetWindowTitle(ost.str(), true);
                return true;
            }
            case Action::DEBUG_STATS2: {
                uint32_t localLines = _priBuffer.getHistoricalRows();
                uint32_t uniqueLines;
                uint32_t globalLines;
                _deduper.getLineStats(uniqueLines, globalLines);
                double   dedupe =
                    uniqueLines == 0 ? 0.0 :
                    static_cast<double>(globalLines) / uniqueLines;

                std::ostringstream ost;
                ost << "local=" << localLines
                    << " global=" << globalLines
                    << " unique=" << uniqueLines
                    << " (dedupe-factor=" << dedupe << ")";
                _observer.terminalSetWindowTitle(ost.str(), true);
                return true;
            }
        }
    }

    return false;
}

void Terminal::fixDamage(Trigger trigger) {
    if (trigger == Trigger::TTY &&          // We're overusing this Damage now.
        _config.scrollOnTtyOutput)
    {
        _buffer->scrollBottomHistory();
    }

    if (_observer.terminalFixDamageBegin()) {
        Region damage;
        bool   scrollbar;
        draw(trigger, damage, scrollbar);

        _observer.terminalFixDamageEnd(damage, scrollbar);
    }
}

void Terminal::draw(Trigger trigger, Region & damage, bool & scrollbar) {
    damage.clear();

    if (trigger == Trigger::FOCUS) {
        if (_modes.get(Mode::SHOW_CURSOR) && !_buffer->isSearching()) {
            _buffer->damageCell();
            _buffer->accumulateDamage(damage);
            _buffer->dispatchCursor(_modes.get(Mode::REVERSE), *this);
        }

        scrollbar = false;
    }
    else {
        if (trigger == Trigger::CLIENT) {
            _buffer->damageViewport(true);
        }
        _buffer->accumulateDamage(damage);
        _buffer->dispatchBg(_modes.get(Mode::REVERSE), *this);
        _buffer->dispatchFg(_modes.get(Mode::REVERSE), *this);

        if (_buffer->isSearching()) {
            _buffer->dispatchSearch(*this);
        }
        else {
            if (_modes.get(Mode::SHOW_CURSOR)) {
                _buffer->dispatchCursor(_modes.get(Mode::REVERSE), *this);
            }
        }

        if (_config.scrollbarVisible) {
            scrollbar = _buffer->getBarDamage();

            if (scrollbar) {
                _observer.terminalDrawScrollbar(_buffer->getTotalRows(),
                                                _buffer->getHistoryOffset(),
                                                _buffer->getRows());
            }
        }
        else {
            scrollbar = false;
        }

        _buffer->resetDamage();
    }
}

void Terminal::write(const uint8_t * data, size_t size) {
    if (_buffer->isSearching()) {
    }
    else {
        _tty.write(data, size);
    }
}

void Terminal::echo(const uint8_t * data, size_t size) {
    while (size != 0) {
        auto c = *data;

        if (c == ESC) {
            processRead(reinterpret_cast<const uint8_t *>("^["), 2);
        }
        else if (c < SPACE) {
            if (c != LF && c != CR && c != HT) {
                c |= 0x40;
                processRead(reinterpret_cast<const uint8_t *>("^"), 1);
            }
            processRead(&c, 1);
        }
        else {
            break;
        }

        ++data; --size;
    }

    if (size != 0) {
        processRead(data, size);
    }

    if (!_config.syncTty) {
        fixDamage(Trigger::TTY);
    }
}

void Terminal::sendMouseButton(int num, ModifierSet modifiers, Pos pos) {
    if (num >= 3) { num += 64 - 3; }

    if (modifiers.get(Modifier::SHIFT))   { num +=  4; }
    if (modifiers.get(Modifier::ALT))     { num +=  8; }
    if (modifiers.get(Modifier::CONTROL)) { num += 16; }

    std::ostringstream ost;
    if (_modes.get(Mode::MOUSE_FORMAT_SGR)) {
        ost << ESC << "[<"
            << num << ';' << pos.col + 1 << ';' << pos.row + 1
            << 'M';
    }
    else if (pos.row < 223 && pos.col < 223) {
        ost << ESC << "[M"
            << static_cast<char>(32 + num)
            << static_cast<char>(32 + pos.col + 1)
            << static_cast<char>(32 + pos.row + 1);
    }
    else {
        // Couldn't deliver it
    }

    const auto & str = ost.str();

    if (!str.empty()) {
        write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
    }
}

void Terminal::resetAll() {
    _buffer->reset();

    _modes.clear();
    _modes.set(Mode::AUTO_WRAP);
    _modes.set(Mode::SHOW_CURSOR);
    _modes.set(Mode::AUTO_REPEAT);
    _modes.set(Mode::ALT_SENDS_ESC);

    _observer.terminalResetTitleAndIcon();
}

void Terminal::processRead(const uint8_t * data, size_t size) {
    for (size_t i = 0; i != size; ++i) {
        switch (_utf8Machine.consume(data[i])) {
            case utf8::Machine::State::ACCEPT:
                processChar(_utf8Machine.seq(), _utf8Machine.length());
                break;
            case utf8::Machine::State::REJECT:
                ERROR("Rejecting UTF-8 data.");
                break;
            default:
                break;
        }
    }
}

void Terminal::processChar(utf8::Seq seq, utf8::Length length) {
    _vtMachine.consume(seq, length);

    if (_config.syncTty) {
        // Note, this is conservative: no damage may require fixing.
        fixDamage(Trigger::TTY);
    }
}

void Terminal::processAttributes(const std::vector<int32_t> & args) {
    ASSERT(!args.empty(), "Empty args.");

    for (size_t i = 0; i != args.size(); ++i) {
        auto v = args[i];

        switch (v) {
            case 0: // Reset/Normal
                _buffer->resetStyle();
                break;
            case 1: // Bold or increased intensity
                _buffer->setAttr(Attr::BOLD);
                break;
            case 2: // Faint (low/decreased intensity)
                _buffer->setAttr(Attr::FAINT);
                break;
            case 3: // Italic: on
                _buffer->setAttr(Attr::ITALIC);
                break;
            case 4: // Underline: Single
                _buffer->setAttr(Attr::UNDERLINE);
                break;
            case 5: // Blink: slow
            case 6: // Blink: rapid
                _buffer->setAttr(Attr::BLINK);
                break;
            case 7: // Inverse (negative)
                _buffer->setAttr(Attr::INVERSE);
                break;
            case 8: // Conceal (not widely supported)
                _buffer->setAttr(Attr::CONCEAL);
                break;
            case 10: // Primary (default) font
                NYI("Primary (default) font");
                break;
            case 11: // 1st alternative font
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19: // 9th alternative font
                NYI(nthStr(v - 10) << " alternative font");
                break;
            case 22: // Normal color or intensity (neither bold nor faint)
                _buffer->unsetAttr(Attr::BOLD);
                _buffer->unsetAttr(Attr::FAINT);
                break;
            case 23: // Not italic
                _buffer->unsetAttr(Attr::ITALIC);
                break;
            case 24: // Underline: None (not singly or doubly underlined)
                _buffer->unsetAttr(Attr::UNDERLINE);
                break;
            case 25: // Blink: off
                _buffer->unsetAttr(Attr::BLINK);
                break;
            case 27: // Clear inverse
                _buffer->unsetAttr(Attr::INVERSE);
                break;
            case 28: // Reveal (conceal off)
                _buffer->unsetAttr(Attr::CONCEAL);
                break;
                // 30..37 (set foreground colour - handled separately)
            case 38:
                // https://github.com/robertknight/konsole/blob/master/user-doc/README.moreColors
                if (i + 1 < args.size()) {
                    i += 1;
                    switch (args[i]) {
                        case 0:
                            NYI("User defined foreground");
                            break;
                        case 1:
                            NYI("Transparent foreground");
                            break;
                        case 2:
                            if (i + 3 < args.size()) {
                                // 24-bit foreground support
                                // ESC[ … 48;2;<r>;<g>;<b> … m Select RGB foreground color
                                _buffer->setFg(UColor::direct(args[i + 1], args[i + 2], args[i + 3]));
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 3:
                            if (i + 3 < args.size()) {
                                NYI("24-bit CMY foreground");
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 4:
                            if (i + 4 < args.size()) {
                                NYI("24-bit CMYK foreground");
                                i += 4;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 5:
                            if (i + 1 < args.size()) {
                                i += 1;
                                auto v2 = args[i];
                                if (v2 >= 0 && v2 < 256) {
                                    _buffer->setFg(UColor::indexed(v2));
                                }
                                else {
                                    ERROR("Colour out of range: " << v2);
                                }
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        default:
                            NYI("Unknown?");
                    }
                }
                break;
            case 39:
                _buffer->setFg(UColor::stock(UColor::Name::TEXT_FG));
                break;
                // 40..47 (set background colour - handled separately)
            case 48:
                // https://github.com/robertknight/konsole/blob/master/user-doc/README.moreColors
                if (i + 1 < args.size()) {
                    i += 1;
                    switch (args[i]) {
                        case 0:
                            NYI("User defined background");
                            break;
                        case 1:
                            NYI("Transparent background");
                            break;
                        case 2:
                            if (i + 3 < args.size()) {
                                // 24-bit background support
                                // ESC[ … 48;2;<r>;<g>;<b> … m Select RGB background color
                                _buffer->setBg(UColor::direct(args[i + 1], args[i + 2], args[i + 3]));
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 3:
                            if (i + 3 < args.size()) {
                                NYI("24-bit CMY background");
                                i += 3;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 4:
                            if (i + 4 < args.size()) {
                                NYI("24-bit CMYK background");
                                i += 4;
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        case 5:
                            if (i + 1 < args.size()) {
                                i += 1;
                                auto v2 = args[i];
                                if (v2 >= 0 && v2 < 256) {
                                    _buffer->setBg(UColor::indexed(v2));
                                }
                                else {
                                    ERROR("Colour out of range: " << v2);
                                }
                            }
                            else {
                                ERROR("Insufficient args");
                                i = args.size() - 1;
                            }
                            break;
                        default:
                            NYI("Unknown?");
                    }
                }
                break;
            case 49:
                _buffer->setBg(UColor::stock(UColor::Name::TEXT_BG));
                break;

            default:
                // 56..59 Reserved
                // 60..64 (ideogram stuff - hardly ever supported)
                // 90..97 Set foreground colour high intensity - handled separately
                // 100..107 Set background colour high intensity - handled separately

                if (v >= 30 && v < 38) {
                    // normal fg
                    _buffer->setFg(UColor::indexed(v - 30));
                }
                else if (v >= 40 && v < 48) {
                    // normal bg
                    _buffer->setBg(UColor::indexed(v - 40));
                }
                else if (v >= 90 && v < 98) {
                    // bright fg
                    _buffer->setFg(UColor::indexed(v - 90 + 8));
                }
                else if (v >= 100 && v < 108) {
                    // bright bg
                    _buffer->setBg(UColor::indexed(v - 100 + 8));
                }
                else if (v >= 256 && v < 512) {
                    _buffer->setFg(UColor::indexed(v - 256));
                }
                else if (v >= 512 && v < 768) {
                    _buffer->setBg(UColor::indexed(v - 512));
                }
                else {
                    //WARNING("Unhandled attribute: " << v);
                }
                break;
        }
    }
}

void Terminal::processModes(uint8_t priv, bool set, const std::vector<int32_t> & args) {
    //PRINT("processModes: priv=" << priv << ", set=" << set << ", args=" << args.front() /*XXX*/);

    for (auto a : args) {
        if (priv == '?') {
            switch (a) {
                case 1: // DECCKM - Cursor Keys Mode - Application / Cursor
                    _modes.setTo(Mode::APPCURSOR, set);
                    break;
                case 2: // DECANM - ANSI/VT52 Mode
                    NYI("DECANM: " << set);
                    /*
                    _cursor.g0 = CS_US;
                    _cursor.g1 = CS_US;
                    _cursor.cs = Cursor::CharSet::G0;
                    */
                    break;
                case 3: // DECCOLM - Column Mode
                    // How much should we reset
                    _buffer->reset();
                    if (set) {
                        // resize 132 columns
                        _observer.terminalResizeBuffer(getRows(), 132);
                    }
                    else {
                        // resize 80 columns
                        _observer.terminalResizeBuffer(getRows(), 80);
                    }
                    break;
                case 4: // DECSCLM - Scroll Mode - Smooth / Jump (IGNORED)
                    NYI("DECSCLM: " << set);
                    break;
                case 5: // DECSCNM - Screen Mode - Reverse / Normal
                    if (_modes.get(Mode::REVERSE) != set) {
                        _modes.setTo(Mode::REVERSE, set);
                        _buffer->damageViewport(false);
                    }
                    break;
                case 6: // DECOM - Origin Mode - Relative / Absolute
                    _modes.setTo(Mode::ORIGIN, set);
                    _buffer->moveCursor(Pos(), _modes.get(Mode::ORIGIN));
                    break;
                case 7: // DECAWM - Auto Wrap Mode
                    _modes.setTo(Mode::AUTO_WRAP, set);
                    break;
                case 8: // DECARM - Auto Repeat Mode
                    _modes.setTo(Mode::AUTO_REPEAT, set);
                    break;
                case 9: // Mouse X10
                    NYI("X10 mouse");
                    break;
                case 12: // CVVIS/att610 - Cursor Very Visible.
                    //NYI("CVVIS/att610: " << set);
                    break;
                case 18: // DECPFF - Printer feed (IGNORED)
                case 19: // DECPEX - Printer extent (IGNORED)
                    NYI("DECPFF/DECPEX: " << set);
                    break;
                case 25: // DECTCEM - Text Cursor Enable Mode
                    _modes.setTo(Mode::SHOW_CURSOR, set);
                    break;
                case 40:
                    // Allow column
                    break;
                case 42: // DECNRCM - National characters (IGNORED)
                    NYI("Ignored: "  << a << ", " << set);
                    break;
                case 47: {
                    Buffer * newBuffer = set ? &_altBuffer : &_priBuffer;
                    if (_buffer != newBuffer) {
                        newBuffer->migrateFrom(*_buffer, false);
                        _buffer = newBuffer;
                    }
                } break;
                case 1000: // Mouse X11 (button press and release)
                    _modes.setTo(Mode::MOUSE_PRESS_RELEASE, set);
                    if (set) {
                        _modes.setTo(Mode::MOUSE_DRAG,   false);
                        _modes.setTo(Mode::MOUSE_MOTION, false);
                        _modes.setTo(Mode::MOUSE_SELECT, false);
                    }
                    break;
                case 1001: // Mouse Highlight (button press and release, but allow select to occur)
                    _modes.setTo(Mode::MOUSE_PRESS_RELEASE, set);
                    _modes.setTo(Mode::MOUSE_SELECT,        set);
                    if (set) {
                        _modes.setTo(Mode::MOUSE_DRAG,   false);
                        _modes.setTo(Mode::MOUSE_MOTION, false);
                    }
                    break;
                case 1002: // Mouse Button (Button press, drag, release)
                    _modes.setTo(Mode::MOUSE_PRESS_RELEASE, set);
                    _modes.setTo(Mode::MOUSE_DRAG,          set);
                    if (set) {
                        _modes.setTo(Mode::MOUSE_MOTION, false);
                        _modes.setTo(Mode::MOUSE_SELECT, false);
                    }
                    break;
                case 1003: // Mouse Any (Button press, drag, release, motion)
                    _modes.setTo(Mode::MOUSE_PRESS_RELEASE, set);
                    _modes.setTo(Mode::MOUSE_DRAG,          set);
                    _modes.setTo(Mode::MOUSE_MOTION,        set);
                    if (set) {
                        _modes.setTo(Mode::MOUSE_SELECT, false);
                    }
                    break;
                case 1004:
                    _modes.setTo(Mode::FOCUS, set);
                    break;
                case 1005: // Mouse Format UTF-8
#if 0
                    _modes.setTo(Mode::MOUSE_FORMAT_UTF8, set);
                    if (set) {
                        _modes.unset(Mode::MOUSE_FORMAT_SGR);
                        _modes.unset(Mode::MOUSE_FORMAT_URXVT);
                    }
#endif
                    break;
                case 1006: // Mouse Format SGR
                    _modes.setTo(Mode::MOUSE_FORMAT_SGR, set);
#if 0
                    if (set) {
                        _modes.unset(Mode::MOUSE_FORMAT_UTF8);
                        _modes.unset(Mode::MOUSE_FORMAT_URXVT);
                    }
#endif
                    break;
                case 1015: // Mouse Format URXVT
#if 0
                    _modes.setTo(Mode::MOUSE_FORMAT_URXVT, set);
                    if (set) {
                        _modes.unset(Mode::MOUSE_FORMAT_UTF8);
                        _modes.unset(Mode::MOUSE_FORMAT_SGR);
                    }
#endif
                    break;
                case 1034: // ssm/rrm, meta mode on/off
                    _modes.setTo(Mode::META_8BIT, set);
                    //PRINT("Setting 8-bit to: " << set);
                    break;
                case 1037: // deleteSendsDel
                    _modes.setTo(Mode::DELETE_SENDS_DEL, set);
                    break;
                case 1039: // altSendsEscape
                    _modes.setTo(Mode::ALT_SENDS_ESC, set);
                    break;
                case 1047: {
                    Buffer * newBuffer = set ? &_altBuffer : &_priBuffer;
                    if (_buffer != newBuffer) {
                        newBuffer->migrateFrom(*_buffer, set);
                        _buffer = newBuffer;
                    }
                } break;
                case 1048:
                    if (set) {
                        _buffer->saveCursor();
                    }
                    else {
                        _buffer->restoreCursor();
                    }
                    break;
                case 1049: { // rmcup/smcup, alternative screen
                    Buffer * newBuffer = set ? &_altBuffer : &_priBuffer;
                    if (_buffer != newBuffer) {
                        if (set) { _buffer->saveCursor(); }
                        newBuffer->migrateFrom(*_buffer, set);
                        _buffer = newBuffer;
                        if (!set) { _buffer->restoreCursor(); }
                    }
                } break;
                case 2004:
                    _modes.setTo(Mode::BRACKETED_PASTE, set);
                    break;
                default:
                    WARNING("erresc: unknown private set/reset mode: " << a);
                    break;
            }
        }
        else if (priv == NUL) {
            switch (a) {
                case 0:  // Error (IGNORED)
                    break;
                case 2:  // KAM - keyboard action
                    _modes.setTo(Mode::KBDLOCK, set);
                    break;
                case 4:  // IRM - Insertion-replacement
                    _modes.setTo(Mode::INSERT, set);
                    break;
                case 12: // SRM - Send/Receive
                    _modes.setTo(Mode::ECHO, !set);
                    break;
                case 20: // LNM - Linefeed/new line
                    _modes.setTo(Mode::CR_ON_LF, set);
                    break;
                default:
                    WARNING("erresc: unknown set/reset mode: " <<  a);
                    break;
            }
        }
        else {
            ERROR("?!");
        }
    }
}

const CharSub * Terminal::lookupCharSub(uint8_t code) {
    switch (code) {
        case '0': // set specg1
            return &CS_SPECIAL;
        case '1': // set altg1
            NYI("Alternate Character rom");
            return nullptr;
        case '2': // set alt specg1
            NYI("Alternate Special Character rom");
            return nullptr;
        case 'A': // set ukg0
            return &CS_UK;
        case 'B': // set usg0
            return &CS_US;
        case '<': // Multinational character set
            NYI("Multinational character set");
            return nullptr;
        case '5': // Finnish
            NYI("Finnish 1");
            return nullptr;
        case 'C': // Finnish
            NYI("Finnish 2");
            return nullptr;
        case 'K': // German
            NYI("German");
            return nullptr;
        default:
            WARNING("Unknown character set: " << Char(code));
            return nullptr;
    }
}
// VtStateMachine::I_Observer implementation:

void Terminal::machineNormal(utf8::Seq seq, utf8::Length UNUSED(length)) throw () {
    _lastSeq = seq;
    _buffer->write(seq, _modes.get(Mode::AUTO_WRAP), _modes.get(Mode::INSERT));
}

void Terminal::machineControl(uint8_t control) throw () {
    switch (control) {
        case BEL:
            _observer.terminalBell();
            break;
        case HT:
            _buffer->tabForward(1);
            break;
        case BS:
            _buffer->backspace(_modes.get(Mode::AUTO_WRAP));
            break;
        case CR:
            _buffer->moveCursor2(true, 0, false, 0);
            break;
        case LF:
            if (_modes.get(Mode::CR_ON_LF)) {
                _buffer->moveCursor2(true, 0, false, 0);
            }
            // Fall-through:
        case FF:
        case VT:
            _buffer->forwardIndex();
            break;
        case SO:
            _buffer->useCharSet(CharSet::G1);
            break;
        case SI:
            _buffer->useCharSet(CharSet::G0);
            break;
        default:
            break;
    }
}

void Terminal::machineSimpleEsc(const SimpleEsc & esc) throw () {
    if (esc.inters.empty()) {
        switch (esc.code) {
            case '7':   // DECSC - Save Cursor
                _buffer->saveCursor();
                break;
            case '8':   // DECRC - Restore Cursor
                _buffer->restoreCursor();
                break;
            case '=':   // DECKPAM - Keypad Application Mode
                _modes.set(Mode::APPKEYPAD);
                break;
            case '>':   // DECKPNM - Keypad Numeric Mode
                _modes.unset(Mode::APPKEYPAD);
                break;
            case 'D':   // IND - Line Feed (opposite of RI)
                _buffer->forwardIndex();
                break;
            case 'E':   // NEL - Next Line
                _buffer->forwardIndex(true);
                break;
            case 'H':   // HTS - Horizontal Tab Stop
                _buffer->setTab();
                break;
            case 'M':   // RI - Reverse Line Feed (opposite of IND)
                _buffer->reverseIndex();
                break;
            case 'N':   // SS2 - Set Single Shift 2
                NYI("SS2");     // Use G2 for next char only
                break;
            case 'O':   // SS3 - Set Single Shift 3
                NYI("SS3");     // Use G3 for next char only
                break;
            case 'Z':   // DECID - Identify Terminal
                write(reinterpret_cast<const uint8_t *>("\x1B[?6c"), 5);
                break;
            case 'c':   // RIS - Reset to initial state
                resetAll();
                break;
            case 'n':   // Designate G2
                _buffer->useCharSet(CharSet::G2);
                break;
            case 'o':   // Designate G3
                _buffer->useCharSet(CharSet::G3);
                break;
            default:
                //WARNING("Unhandled: " << esc);
                break;
        }
    }
    else if (esc.inters.size() == 1) {
        switch (esc.inters.front()) {
            case '#':
                switch (esc.code) {
                    case '3': // DECDHL - Double height/width (top half of char)
                        NYI("Double height (top)");
                        break;
                    case '4': // DECDHL - Double height/width (bottom half of char)
                        NYI("Double height (bottom)");
                        break;
                    case '5': // DECSWL - Single height/width
                        break;
                    case '6': // DECDWL - Double width
                        NYI("Double width");
                        break;
                    case '8': // DECALN - Alignment
                        _buffer->testPattern();
                        break;
                    default:
                        //WARNING("Unhandled: " << esc);
                        break;
                }
                break;
            case '(':
                if (const CharSub * charSub = lookupCharSub(esc.code)) {
                    _buffer->setCharSub(CharSet::G0, charSub);
                }
                break;
            case ')':
                if (const CharSub * charSub = lookupCharSub(esc.code)) {
                    _buffer->setCharSub(CharSet::G1, charSub);
                }
                break;
            case '*':
                if (const CharSub * charSub = lookupCharSub(esc.code)) {
                    _buffer->setCharSub(CharSet::G2, charSub);
                }
                break;
            case '+':
                if (const CharSub * charSub = lookupCharSub(esc.code)) {
                    _buffer->setCharSub(CharSet::G3, charSub);
                }
                break;
            default:
                //WARNING("Unhandled: " << esc);
                break;
        }
    }
    else {
        //WARNING("Unhandled: " << esc);
    }
}

void Terminal::machineCsiEsc(const CsiEsc & esc) throw () {
    if (esc.inters.empty()) {
        switch (esc.mode) {
            case '@': { // ICH - Insert Character
                _buffer->insertCells(nthArgNonZero(esc.args, 0, 1));
                break;
            }
            case 'A': // CUU - Cursor Up
                _buffer->moveCursor2(true, -nthArgNonZero(esc.args, 0, 1), true, 0);
                break;
            case 'B': // CUD - Cursor Down
                _buffer->moveCursor2(true, nthArgNonZero(esc.args, 0, 1), true, 0);
                break;
            case 'C': // CUF - Cursor Forward
                _buffer->moveCursor2(true, 0, true, nthArgNonZero(esc.args, 0, 1));
                break;
            case 'D': // CUB - Cursor Backward
                _buffer->moveCursor2(true, 0, true, -nthArgNonZero(esc.args, 0, 1));
                break;
            case 'E': // CNL - Cursor Next Line
                _buffer->moveCursor2(true, nthArgNonZero(esc.args, 0, 1), false, 0);
                break;
            case 'F': // CPL - Cursor Preceding Line
                _buffer->moveCursor2(true, -nthArgNonZero(esc.args, 0, 1), false, 0);
                break;
            case 'G': // CHA - Cursor Horizontal Absolute
                _buffer->moveCursor2(true, 0, false, nthArgNonZero(esc.args, 0, 1) - 1);
                break;
            case 'H': // CUP - Cursor Position
                _buffer->moveCursor(Pos(nthArg(esc.args, 0, 1) - 1, nthArg(esc.args, 1, 1) - 1),
                                    _modes.get(Mode::ORIGIN));
                break;
            case 'I': // CHT - Cursor Forward Tabulation
                _buffer->tabForward(nthArgNonZero(esc.args, 0, 1));
                break;
            case 'J': // ED - Erase Data
                // Clear screen.
                switch (nthArg(esc.args, 0)) {
                    default:
                    case 0: // ED0 - Below
                        _buffer->clearBelow();
                        break;
                    case 1: // ED1 - Above
                        _buffer->clearAbove();
                        break;
                    case 2: // ED2 - All
                        _buffer->clear();
                        _buffer->moveCursor(Pos(), _modes.get(Mode::ORIGIN));
                        break;
                }
                break;
            case 'K':   // EL - Erase line
                switch (nthArg(esc.args, 0)) {
                    default:
                    case 0: // EL0 - Right (inclusive of cursor position)
                        _buffer->clearLineRight();
                        break;
                    case 1: // EL1 - Left (inclusive of cursor position)
                        _buffer->clearLineLeft();
                        break;
                    case 2: // EL2 - All
                        _buffer->clearLine();
                        break;
                }
                break;
            case 'L': // IL - Insert Lines
                _buffer->insertLines(nthArgNonZero(esc.args, 0, 1));
                break;
            case 'M': // DL - Delete Lines
                _buffer->eraseLines(nthArgNonZero(esc.args, 0, 1));
                break;
            case 'P': { // DCH - Delete Character
                _buffer->eraseCells(nthArgNonZero(esc.args, 0, 1));
                break;
            }
            case 'S': // SU - Scroll Up
                _buffer->scrollUpMargins(nthArgNonZero(esc.args, 0, 1));
                break;
            case 'T': // SD - Scroll Down
                _buffer->scrollDownMargins(nthArgNonZero(esc.args, 0, 1));
                break;
            case 'W': // CTC - Tabulator functions
                switch (nthArg(esc.args, 0, 0)) {
                    case 0:
                        _buffer->setTab();     // HTS
                        break;
                    case 2:
                        _buffer->unsetTab();   // TBC
                        break;
                    case 5:
                        _buffer->clearTabs();
                        break;
                    default:
                        goto default_;
                }
                break;
            case 'X': { // ECH - Erase Char
                _buffer->blankCells(nthArgNonZero(esc.args, 0, 1));
                break;
            }
            case 'Z': // CBT - Cursor Backward Tabulation
                _buffer->tabBackward(nthArgNonZero(esc.args, 0, 1));
                break;
            case '`': // HPA
                _buffer->moveCursor2(true, 0, false, nthArgNonZero(esc.args, 0, 1) - 1);
                break;
            case 'a': // HPR - Horizontal Position Relative
                _buffer->moveCursor2(true, 0, true, nthArgNonZero(esc.args, 0, 1));
                break;
            case 'b': { // REP
                if (_lastSeq.lead() != NUL) {
                    auto count = nthArgNonZero(esc.args, 0, 1);
                    for (auto i = 0; i != count; ++i) {
                        machineNormal(_lastSeq, utf8::leadLength(_lastSeq.lead()));
                    }
                    _lastSeq.clear();
                }
                break;
            }
            case 'c': // Primary DA
                write(reinterpret_cast<const uint8_t *>("\x1B[?6c"), 5);
                break;
            case 'd': // VPA - Vertical Position Absolute
                _buffer->moveCursor2(false, nthArg(esc.args, 0, 1) - 1, true, 0);
                break;
            case 'e': // VPR - Vertical Position Relative
                _buffer->moveCursor2(true, nthArgNonZero(esc.args, 0, 1), true, 0);
                break;
            case 'f': // HVP - Horizontal and Vertical Position
                _buffer->moveCursor(Pos(nthArg(esc.args, 0, 1) - 1, nthArg(esc.args, 1, 1) - 1),
                                    _modes.get(Mode::ORIGIN));
                break;
            case 'g': // TBC
                switch (nthArg(esc.args, 0, 0)) {
                    case 0:
                        _buffer->unsetTab();
                        break;
                    case 3:
                        _buffer->clearTabs();
                        break;
                    default:
                        goto default_;
                }
                break;
            case 'h': // SM
                //PRINT("CSI: Set terminal mode: " << strArgs(esc.args));
                processModes(esc.priv, true, esc.args);
                break;
            case 'l': // RM
                //PRINT("CSI: Reset terminal mode: " << strArgs(esc.args));
                processModes(esc.priv, false, esc.args);
                break;
            case 'm': // SGR - Select Graphic Rendition
                if (esc.args.empty()) {
                    std::vector<int32_t> args2;
                    args2.push_back(0);
                    processAttributes(args2);
                }
                else {
                    processAttributes(esc.args);
                }
                break;
            case 'n': // DSR - Device Status Report
                if (esc.args.empty()) {
                    // QDC - Query Device Code
                    // RDC - Report Device Code: <ESC>[{code}0c
                    NYI("What code should I send?");
                }
                else {
                    switch (nthArg(esc.args, 0)) {
                        case 5: {
                            // QDS - Query Device Status
                            // RDO - Report Device OK: <ESC>[0n
                            std::ostringstream ost;
                            ost << ESC << "[0n";
                            const auto & str = ost.str();
                            write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
                            break;
                        }
                        case 6: {
                            // QCP - Query Cursor Position
                            // RCP - Report Cursor Position
                            auto pos = _buffer->getCursorPos();
                            std::ostringstream ost;
                            ost << ESC << '['
                                << pos.row + 1 << ';'
                                << pos.col + 1 << 'R';
                            const auto & str = ost.str();
                            write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
                            break;
                        }
                        case 7: {
                            // Ps = 7   Request Display Name
                            std::ostringstream ost;
                            ost << _observer.terminalGetDisplayName() << LF;
                            const auto & str = ost.str();
                            write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
                            break;
                        }
                        case 8: {
                            // Ps = 8   Request Version Number (place in window title)
                            _observer.terminalSetWindowTitle("Terminol " VERSION, true);
                            break;
                        }
                        case 15: {
                            // TPS - Test Printer Status
                            std::ostringstream ost;
                            ost << ESC << "[?13n";
                            const auto & str = ost.str();
                            write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
                            break;
                        }
                        case 25: {
                            NYI("UDK status");
                            break;
                        }
                        case 26: {
                            NYI("Keyboard status");
                            break;
                        }
                        default:
                            //WARNING("Unhandled: " << esc);
                            break;
                    }
                }
                break;
            case 'p':
                if (esc.priv == '!') {
                    // DECSTR - Soft Terminal Reset
                    NYI("DECSTR");
                }
                else {
                    // XXX vttest gives soft-reset without priv == '!'
                    goto default_;
                }
                break;
            case 'q': // DECSCA - Select Character Protection Attribute
                // OR IS THIS DECLL0/DECLL1/etc
                NYI("DECSCA");
                break;
            case 'r': // DECSTBM - Set Top and Bottom Margins (scrolling)
                if (esc.priv) {
                    goto default_;
                }
                else {
                    if (esc.args.empty()) {
                        _buffer->resetMargins();
                    }
                    else {
                        // http://www.vt100.net/docs/vt510-rm/DECSTBM
                        auto top    = nthArgNonZero(esc.args, 0, 1) - 1;
                        auto bottom = nthArgNonZero(esc.args, 1, _buffer->getRows()) - 1;

                        top    = clamp<int32_t>(top,    0, _buffer->getRows() - 1);
                        bottom = clamp<int32_t>(bottom, 0, _buffer->getRows() - 1);

                        _buffer->setMargins(top, bottom + 1);
                    }
                    _buffer->moveCursor(Pos(), _modes.get(Mode::ORIGIN));
                }
                break;
            case 's': // save cursor
                _buffer->saveCursor();
                break;
            case 't': // window ops?
                // FIXME see 'Window Operations' in man 7 urxvt.
                NYI("Window ops");
                break;
            case 'u': // restore cursor
                _buffer->restoreCursor();
                break;
            case 'y': // DECTST
                NYI("DECTST");
                break;
default_:
            default:
                //WARNING("Unhandled: " << esc);
                break;
        }
    }
    else if (esc.inters.size() == 1) {
        auto i = esc.inters.back();

        if (i == '$') {
            switch (esc.mode) {
                case 'p': { // DECRQM
                    if (esc.priv == '?') {}

                    auto m = nthArgNonZero(esc.args, 0, 1);

                    std::ostringstream ost;
                    ost << ESC << "[?" << m << ";0$y";
                    const auto & str = ost.str();
                    write(reinterpret_cast<const uint8_t *>(str.data()), str.size());
                    break;
                }
                default:
                    break;
            }
        }
    }
    else {
    }
}

void Terminal::machineDcsEsc(const DcsEsc & UNUSED(esc)) throw () {
    //WARNING("Unhandled: " << esc);
}

void Terminal::machineOscEsc(const OscEsc & esc) throw () {
    if (!esc.args.empty()) {
        try {
            switch (unstringify<int>(esc.args[0])) {
                case 0: // Icon name and window title
                    if (esc.args.size() > 1) {
                        _observer.terminalSetIconName(esc.args[1]);
                        _observer.terminalSetWindowTitle(esc.args[1], false);
                    }
                    break;
                case 1: // Icon name
                    if (esc.args.size() > 1) {
                        _observer.terminalSetIconName(esc.args[1]);
                    }
                    break;
                case 2: // Window title
                    if (esc.args.size() > 1) {
                        _observer.terminalSetWindowTitle(esc.args[1], false);
                    }
                    break;
                case 55:
                    NYI("Log history to file");
                    break;
                case 112:
                    // tmux gives us this...
                    break;
                case 666: // terminol extension (fix the damage)
                    fixDamage(Trigger::TTY);
                    break;
                case 667: { // terminol extension (random resize)
                    int cols = 1 + (random() % 120);
                    int rows = 1 + (random() % 40);
                    _observer.terminalResizeBuffer(rows, cols);
                    break;
                }
                default:
                    // TODO consult http://rtfm.etla.org/xterm/ctlseq.html AND man 7 urxvt.
                    //WARNING("Unhandled: " << esc);
                    break;
            }
        }
        catch (const ParseError & ex) {
            ERROR(ex.message);
        }
    }
}

// Tty::I_Observer imlementation:

void Terminal::ttyData(const uint8_t * data, size_t size) throw () {
    processRead(data, size);
}

void Terminal::ttySync() throw () {
    fixDamage(Trigger::TTY);
}

void Terminal::ttyReaped(int status) throw () {
    _observer.terminalReaped(status);
}

// Buffer::I_Renderer implementation

void Terminal::bufferDrawBg(Pos     pos,
                            int16_t count,
                            UColor  color) throw () {
    _observer.terminalDrawBg(pos, count, color);
}

void Terminal::bufferDrawFg(Pos             pos,
                            int16_t         count,
                            UColor          color,
                            AttrSet         attrs,
                            const uint8_t * str,
                            size_t          size) throw () {
    _observer.terminalDrawFg(pos, count, color, attrs, str, size);
}

void Terminal::bufferDrawCursor(Pos             pos,
                                UColor          fg,
                                UColor          bg,
                                AttrSet         attrs,
                                const uint8_t * str,
                                size_t          size,
                                bool            wrapNext) throw () {
    _observer.terminalDrawCursor(pos, fg, bg, attrs, str, size, wrapNext, _focused);
}

std::ostream & operator << (std::ostream & ost, Terminal::Button button) {
    switch (button) {
        case Terminal::Button::LEFT:
            return ost << "left";
        case Terminal::Button::MIDDLE:
            return ost << "middle";
        case Terminal::Button::RIGHT:
            return ost << "right";
    }

    FATAL("Unreachable.");
}

std::ostream & operator << (std::ostream & ost, Terminal::ScrollDir dir) {
    switch (dir) {
        case Terminal::ScrollDir::UP:
            return ost << "up";
        case Terminal::ScrollDir::DOWN:
            return ost << "down";
    }

    FATAL("Unreachable.");
}
