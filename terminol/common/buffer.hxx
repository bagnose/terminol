// vi:noai:sw=4

#ifndef COMMON__BUFFER__HXX
#define COMMON__BUFFER__HXX

#include "terminol/common/data_types.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/deduper_interface.hxx"
#include "terminol/support/escape.hxx"
#include "terminol/support/regex.hxx"

#include <deque>
#include <vector>
#include <iomanip>

// Character-Substitution
class CharSub {
    const utf8::Seq * _seqs;
    size_t            _offset;
    size_t            _size;
    bool              _special;

public:
    CharSub() : _seqs(nullptr), _offset(0), _size(0), _special(false) {}

    CharSub(const utf8::Seq * seqs, size_t offset, size_t size, bool special = false) :
        _seqs(seqs), _offset(offset), _size(size), _special(special) {}

    // Bold and italic attributes are disabled from 'special' char subs.
    bool isSpecial() const { return _special; }

    void translate(utf8::Seq & seq) const {
        if (utf8::leadLength(seq.lead()) == utf8::Length::L1) {
            auto ascii = seq.lead();
            if (ascii >= _offset && ascii < _offset + _size) {
                seq = _seqs[ascii - _offset];
            }
        }
    }
};

// Character-Set
enum class CharSet { G0, G1 };

// Tab-Direction
enum class TabDir { FORWARD, BACKWARD };

//
//
//

class Buffer {
    // Absolute-Position, able to refer to historical AND active lines.
    struct APos {
        int32_t row; // >= 0 --> _active, < 0 --> _history
        int16_t col;

        APos() : row(0), col(0) {}
        APos(int32_t row_, int16_t col_) : row(row_), col(col_) {}
        APos(Pos pos, uint32_t offset) : row(pos.row - offset), col(pos.col) {}
    };

    friend bool operator == (const APos & lhs, const APos & rhs) {
        return lhs.row == rhs.row && lhs.col == rhs.col;
    }

    friend bool operator != (const APos & lhs, const APos & rhs) {
        return !(lhs == rhs);
    }

    friend bool operator <  (const APos & lhs, const APos & rhs) {
        return
            (lhs.row <  rhs.row) ||
            (lhs.row == rhs.row && lhs.col < rhs.col);
    }

    // Handed-Absolute-Position, where handed means left or right side of cell.
    struct HAPos {
        APos apos;
        Hand hand;

        HAPos() : apos(), hand(Hand::LEFT) {}
        HAPos(HPos hpos, uint32_t offset) : apos(hpos.pos, offset), hand(hpos.hand) {}
        HAPos(int32_t row_, int16_t col_, Hand hand_ = Hand::LEFT) :
            apos(row_, col_), hand(hand_) {}
    };

    friend bool operator == (const HAPos & lhs, const HAPos & rhs) {
        return lhs.apos == rhs.apos && lhs.hand == rhs.hand;
    }

    friend bool operator != (const HAPos & lhs, const HAPos & rhs) {
        return !(lhs == rhs);
    }

    friend bool operator <  (const HAPos & lhs, const HAPos & rhs) {
        return
            (lhs.apos.row <  rhs.apos.row) ||
            (lhs.apos.row == rhs.apos.row && lhs.apos.col <  rhs.apos.col) ||
            (lhs.apos.row == rhs.apos.row && lhs.apos.col == rhs.apos.col && lhs.hand < rhs.hand);
    }

    // Historical-Line
    struct HLine {
        uint32_t index;             // index into _tags (adjusted by _lostTags)
        uint16_t seqnum;            // continuation number, 0 -> first line
        uint16_t size;

        HLine(uint32_t index_, uint16_t seqnum_, uint16_t size_) :
            index(index_), seqnum(seqnum_), size(size_) {}
    };

    // Active-Line
    struct ALine {
        std::vector<Cell> cells;    // active lines have a greater/equal capacity to their wrap/size
        bool              cont;     // does this line continue on the next line?
        int16_t           wrap;     // wrappable index, <= cells.size()

        explicit ALine(int16_t cols) :
            cells(cols, Cell::blank()), cont(false), wrap(0) {}

        ALine(std::vector<Cell> & cells_, bool cont_, int16_t wrap_, int16_t cols) :
            cells(std::move(cells_)), cont(cont_), wrap(wrap_)
        {
            ASSERT(wrap_ <= cols, "");
            cells.resize(cols, Cell::blank());
        }

        void clear() {
            cont = false;
            wrap = 0;
            std::fill(cells.begin(), cells.end(), Cell::blank());
        }

        bool isBlank() const {
            for (const auto & c : cells) {
                if (c != Cell::blank()) { return false; }
            }
            return true;
        }
    };

    // Damage for a visible line (active or historical, but in the viewport)
    struct Damage {
        int16_t begin;
        int16_t end;

        Damage() : begin(0), end(0) {}

        void damageSet(int16_t begin_, int16_t end_) {
            begin = begin_;
            end   = end_;
        }

        void damageAdd(int16_t begin_, int16_t end_) {
            ASSERT(begin_ <= end_, "");

            if (begin != end) {
                begin = std::min(begin, begin_);
                end   = std::max(end,   end_);
            }
            else {
                begin = begin_;
                end   = end_;
            }
        }

        void reset() {
            *this = Damage();
        }
    };

    // Cursor
    struct Cursor {
        Pos             pos;
        Style           style;
        bool            wrapNext;

        CharSet         charSet;
        const CharSub * g0;
        const CharSub * g1;

        const CharSub * getCharSub() const {
            switch (charSet) {
                case CharSet::G0:
                    return g0;
                case CharSet::G1:
                    return g1;
            }

            FATAL("Unreachable");
        }

        void setCharSub(CharSet charSet_, const CharSub * charSub) {
            switch (charSet_) {
                case CharSet::G0:
                    g0 = charSub;
                    return;
                case CharSet::G1:
                    g1 = charSub;
                    return;
            }

            FATAL("Unreachable");
        }

        Cursor(const CharSub * g0_, const CharSub * g1_) :
            pos(), style(), wrapNext(false), charSet(CharSet::G0), g0(g0_), g1(g1_) {}
    };

    const Config               & _config;
    I_Deduper                  & _deduper;
    std::deque<I_Deduper::Tag>   _tags;
    std::vector<Cell>            _pending;
    std::deque<HLine>            _history;
    std::deque<ALine>            _active;
    std::vector<Damage>          _damage;           // viewport relative damage
    std::vector<bool>            _tabs;
    uint32_t                     _scrollOffset;     // 0 -> scroll bottom
    uint32_t                     _historyLimit;
    uint32_t                     _lostTags;
    int16_t                      _cols;
    int16_t                      _marginBegin;
    int16_t                      _marginEnd;
    bool                         _barDamage;
    HAPos                        _selectMark;
    HAPos                        _selectDelim;
    Cursor                       _cursor;
    Cursor                       _savedCursor;

public:
    Buffer(const Config  & config,
           I_Deduper     & deduper,
           int16_t         rows,
           int16_t         cols,
           uint32_t        historyLimit,
           const CharSub * g0,
           const CharSub * g1) :
        _config(config),
        _deduper(deduper),
        _tags(),
        _pending(),
        _history(),
        _active(rows, ALine(cols)),
        _damage(rows),
        _tabs(cols),
        _scrollOffset(0),
        _historyLimit(historyLimit),
        _lostTags(0),
        _cols(cols),
        _barDamage(true),
        _selectMark(),
        _selectDelim(),
        _cursor(g0, g1),
        _savedCursor(g0, g1)
    {
        resetMargins();
        resetTabs();
    }

    ~Buffer() {
        for (auto tag : _tags) {
            if (tag != I_Deduper::invalidTag()) {
                _deduper.remove(tag);
            }
        }
    }

    int16_t  getRows() const { return static_cast<int16_t>(_active.size()); }
    int16_t  getCols() const { return _cols; }

    uint32_t getHistoricalRows() const { return _history.size(); }
    uint32_t getTotalRows() const { return _history.size() + _active.size(); }
    uint32_t getHistoryOffset() const { return _history.size() - _scrollOffset; }
    uint32_t getScrollOffset() const { return _scrollOffset; }
    bool     getBarDamage() const { return _barDamage; }

    void markSelection(HPos hpos) {
        damageSelection();
        _selectMark = _selectDelim = HAPos(hpos, _scrollOffset);
    }

    void delimitSelection(HPos hpos, bool initial) {
        damageSelection();

        HAPos hapos(hpos, _scrollOffset);

        if (initial) {
            if (_selectDelim < _selectMark) {
                std::swap(_selectMark, _selectDelim);
            }

            auto deltaRow = _selectDelim.apos.row - _selectMark.apos.row;
            auto deltaCol = _selectDelim.apos.col - _selectMark.apos.col;

            auto row = _selectMark.apos.row + deltaRow / 2;
            auto col = _selectMark.apos.col + deltaCol / 2;

            if (deltaRow % 2) { col += getCols() / 2; }

            if (col < 0)          { col += getCols(); --row; }
            if (col >= getCols()) { col -= getCols(); ++row; }

            ASSERT(col >= 0, "");
            ASSERT(col < getCols(), "");

            HAPos centre(row, col);

            if (hapos < centre) {
                std::swap(_selectMark, _selectDelim);
            }
        }

        _selectDelim = hapos;
        damageSelection();
    }

    void expandSelection(HPos hpos, int level) {
        level = level % 4;

        damageSelection();

        if (_selectDelim < _selectMark) {
            std::swap(_selectMark, _selectDelim);
        }

        HAPos hapos(hpos, _scrollOffset);

        if (hapos.apos.row < 0) {
            // Historical

            auto & hline = _history[_history.size() + hapos.apos.row];
            auto   tag   = _tags[hline.index - _lostTags];
            auto & cells = tag == I_Deduper::invalidTag() ? _pending : _deduper.lookup(tag);

            auto   left  = static_cast<uint32_t>(hline.seqnum) * getCols() + hapos.apos.col;
            auto   right = left;

            if (level == 1) {
                _selectMark = _selectDelim = hapos;
            }
            else if (level == 3 || cells[left].seq.lead() == ' ') {
                _selectMark  = HAPos(hapos.apos.row - hline.seqnum, 0, Hand::LEFT);
                _selectDelim = HAPos(hapos.apos.row + (cells.size() / getCols()) - hline.seqnum,
                                     getCols() - 1, Hand::RIGHT);
            }
            else {
                Regex regex("[" + _config.cutChars + "]");

                _selectMark      = hapos;
                _selectMark.hand = Hand::LEFT;

                while (left != 0) {
                    auto seq = cells[left - 1].seq;
                    std::string s(seq.bytes, seq.bytes + utf8::leadLength(seq.lead()));
                    if (regex.match(s).empty()) { break; }

                    --left;

                    if (_selectMark.apos.col == 0) {
                        --_selectMark.apos.row;
                        _selectMark.apos.col = getCols() - 1;
                    }
                    else {
                        --_selectMark.apos.col;
                    }
                }

                _selectDelim      = hapos;
                _selectDelim.hand = Hand::RIGHT;

                while (right != cells.size() - 1) {
                    auto seq = cells[right + 1].seq;
                    std::string s(seq.bytes, seq.bytes + utf8::leadLength(seq.lead()));
                    if (regex.match(s).empty()) { break; }

                    ++right;

                    if (_selectDelim.apos.col == getCols() - 1) {
                        _selectDelim.apos.col = 0;
                        ++_selectDelim.apos.row;
                    }
                    else {
                        ++_selectDelim.apos.col;
                    }
                }
            }
        }
        else {
            // Active

            int16_t row  = hapos.apos.row;
            int16_t col  = hapos.apos.col;

            auto & aline = _active[row];
            auto & cells = aline.cells;

            if (level == 1) {
                _selectMark = _selectDelim = hapos;
            }
            else if (level == 3 || cells[col].seq.lead() == ' ') {
                _selectMark  = HAPos(hapos.apos.row, 0, Hand::LEFT);
                _selectDelim = HAPos(hapos.apos.row, getCols() - 1, Hand::RIGHT);
            }
            else {
                Regex regex("[" + _config.cutChars + "]");

                _selectMark      = hapos;
                _selectMark.hand = Hand::LEFT;

                while (_selectMark.apos.col != 0) {
                    auto seq = cells[_selectMark.apos.col - 1].seq;
                    std::string s(seq.bytes, seq.bytes + utf8::leadLength(seq.lead()));
                    if (regex.match(s).empty()) { break; }
                    --_selectMark.apos.col;
                }

                _selectDelim      = hapos;
                _selectDelim.hand = Hand::RIGHT;

                while (_selectDelim.apos.col != static_cast<int16_t>(cells.size() - 1)) {
                    auto seq = cells[_selectDelim.apos.col + 1].seq;
                    std::string s(seq.bytes, seq.bytes + utf8::leadLength(seq.lead()));
                    if (regex.match(s).empty()) { break; }
                    ++_selectDelim.apos.col;
                }
            }
        }

        damageSelection();
    }

    void clearSelection() {
        damageSelection();
        _selectDelim = _selectMark = HAPos();
    }

    bool getSelectedText(std::string & text) const {
        APos begin, end;

        if (normaliseSelection(begin, end)) {
            for (auto i = begin; i.row <= end.row; ++i.row, i.col = 0) {
                const std::vector<Cell> * cellsPtr;
                uint32_t                  offset;
                int16_t                   wrap;

                if (i.row < 0) {
                    auto & hline = _history[_history.size() + i.row];
                    auto   tag   = _tags[hline.index - _lostTags];

                    if (tag == I_Deduper::invalidTag()) {
                        cellsPtr = &_pending;
                    }
                    else {
                        cellsPtr = &_deduper.lookup(tag);
                    }

                    offset = hline.seqnum * getCols();
                    wrap   = cellsPtr->size() - offset;
                }
                else {
                    auto & aline = _active[i.row];

                    cellsPtr = &aline.cells;
                    offset   = 0;
                    wrap     = aline.wrap;
                }

                auto & cells = *cellsPtr;

                for (; i.col < getCols() && (i.row < end.row || i.col != end.col); ++i.col) {
                    if (i.col == wrap) { text.push_back('\n'); break; }
                    if (i.col + offset == cells.size()) { break; }

                    auto & cell = cells[i.col + offset];
                    auto   seq  = cell.seq;
                    std::copy(&seq.bytes[0],
                              &seq.bytes[utf8::leadLength(seq.lead())],
                              back_inserter(text));
                }
            }

            return true;
        }
        else {
            return false;
        }
    }

    void clearHistory() {
        if (!_history.empty()) {
            for (auto tag : _tags) {
                if (tag != I_Deduper::invalidTag()) {
                    _deduper.remove(tag);
                }
            }

            _tags.clear();
            _history.clear();
            _pending.clear();

            if (_scrollOffset == 0) {
                _barDamage = true;
            }
            else {
                _scrollOffset = 0;
                damageViewport(true);
            }
        }
    }

    bool scrollUpHistory(uint16_t rows) {
        auto oldScrollOffset = _scrollOffset;

        if (_scrollOffset + rows > getHistoricalRows()) {
            _scrollOffset = getHistoricalRows();
        }
        else {
            _scrollOffset += rows;
        }

        if (_scrollOffset != oldScrollOffset) {
            damageViewport(true);
            return true;
        }
        else {
            return false;
        }
    }

    bool scrollDownHistory(uint16_t rows) {
        auto oldScrollOffset = _scrollOffset;

        if (rows > _scrollOffset) {
            _scrollOffset = 0;
        }
        else {
            _scrollOffset -= rows;
        }

        if (_scrollOffset != oldScrollOffset) {
            damageViewport(true);
            return true;
        }
        else {
            return false;
        }
    }

    bool scrollTopHistory() {
        if (_scrollOffset != getHistoricalRows()) {
            _scrollOffset = getHistoricalRows();
            damageViewport(true);
            return true;
        }
        else {
            return false;
        }
    }

    bool scrollBottomHistory() {
        if (_scrollOffset != 0) {
            _scrollOffset = 0;
            damageViewport(true);
            return true;
        }
        else {
            return false;
        }
    }

    Pos getCursorPos() const { return _cursor.pos; }

    void migrateFrom(Buffer & other, bool clear_) {
        other.clearSelection();
        _cursor          = other._cursor;
        _cursor.wrapNext = false;
        if (clear_) {
            clear();
            _barDamage = true;
        }
        else {
            damageViewport(true);
        }
    }

    void resetDamage() {
        for (auto & d : _damage) {
            d.reset();
        }
        _barDamage = false;
    }

    void write(utf8::Seq seq, bool autoWrap, bool insert) {
        damageCell();

        auto cs = _cursor.getCharSub();
        cs->translate(seq);

        if (autoWrap && _cursor.wrapNext) {
            _cursor.wrapNext = false;
            _active[_cursor.pos.row].cont = true; // continues on next line
            ASSERT(_cursor.pos.col == _cols - 1, "col=" << _cursor.pos.col << ", _cols-1=" << _cols - 1);
            ASSERT(_active[_cursor.pos.row].wrap == _cols, "wrap=" << _active[_cursor.pos.row].wrap << ", _cols=" << _cols);
            _active[_cursor.pos.row].wrap = _cols;

            if (_cursor.pos.row == _marginEnd - 1) {
                addLine();
                moveCursor2(true, 0, false, 0);
            }
            else {
                moveCursor2(true, 1, false, 0);
            }
        }
        else if (insert) {
            insertCells(1);
        }

        APos begin, end;
        if (normaliseSelection(begin, end)) {
            APos cpos(_cursor.pos, 0);

            if (!(cpos < begin) && cpos < end) {
                clearSelection();
            }
        }

        auto & line = _active[_cursor.pos.row];

        if (cs->isSpecial()) {
            auto style = _cursor.style;
            style.attrs.unset(Attr::BOLD);
            style.attrs.unset(Attr::ITALIC);
            line.cells[_cursor.pos.col] = Cell::utf8(seq, style);
        }
        else {
            auto & style = _cursor.style;
            line.cells[_cursor.pos.col] = Cell::utf8(seq, style);
        }

        line.wrap = std::max<int16_t>(line.wrap, _cursor.pos.col + 1);
        ASSERT(line.wrap <= getCols(), "");

        if (_cursor.pos.col == getCols() - 1) {
            _cursor.wrapNext = true;
        }
        else {
            ++_cursor.pos.col;
        }

        damageCell();
    }

    void backspace(bool autoWrap) {
        if (_cursor.wrapNext && !_config.traditionalWrapping) {
            _cursor.wrapNext = false;
        }
        else {
            if (_cursor.pos.col == 0) {
                if (!_config.traditionalWrapping && autoWrap) {
                    if (_cursor.pos.row > _marginBegin) {
                        moveCursor2(true, -1, false, getCols() - 1);
                    }
                }
            }
            else {
                moveCursor2(true, 0, true, -1);
            }
        }
    }

    void forwardIndex(bool resetCol = false) {
        if (_cursor.pos.row == _marginEnd - 1) {
            addLine();
            moveCursor2(true, 0, !resetCol, 0);
        }
        else {
            moveCursor2(true, 1, !resetCol, 0);
        }
    }

    void reverseIndex() {
        if (_cursor.pos.row == _marginBegin) {
            _cursor.wrapNext = false;
            insertLinesAt(_marginBegin, 1);
        }
        else {
            moveCursor2(true, -1, true, 0);
        }
    }

    void setTab() {
        _tabs[_cursor.pos.col] = true;
    }

    void unsetTab() {
        _tabs[_cursor.pos.col] = false;
    }

    void clearTabs() {
        std::fill(_tabs.begin(), _tabs.end(), false);
    }

    void moveCursor(Pos pos, bool marginRelative = false) {
        _cursor.wrapNext = false;
        damageCell();

        if (marginRelative) {
            pos.row += _marginBegin;
        }

        _cursor.pos = pos;

        _cursor.pos.row = clamp<int16_t>(_cursor.pos.row, _marginBegin, _marginEnd - 1);
        _cursor.pos.col = clamp<int16_t>(_cursor.pos.col, 0, getCols() - 1);

        damageCell();
    }

    void moveCursor2(bool rowRelative, int16_t row,
                     bool colRelative, int16_t col) {
        _cursor.wrapNext = false;
        damageCell();

        _cursor.pos.row = rowRelative ? _cursor.pos.row + row : row;
        _cursor.pos.col = colRelative ? _cursor.pos.col + col : col;

        _cursor.pos.row = clamp<int16_t>(_cursor.pos.row, _marginBegin, _marginEnd - 1);
        _cursor.pos.col = clamp<int16_t>(_cursor.pos.col, 0, getCols() - 1);

        damageCell();
    }

    void saveCursor() {
        _savedCursor = _cursor;
        _savedCursor.wrapNext = false;      // XXX ??
    }

    void restoreCursor() {
        damageCell();
        _cursor = _savedCursor;
        ASSERT(_cursor.pos.row < getRows(), "");
        ASSERT(_cursor.pos.col < getCols(), "");
        damageCell();
    }

    void resizeClip(int16_t rows, int16_t cols) {
        ASSERT(rows > 0 && cols > 0, "");
        ASSERT(_cursor.pos.row >= 0 && _cursor.pos.row < getRows(), "");
        ASSERT(_cursor.pos.col >= 0 && _cursor.pos.col < getCols(), "");

        clearSelection();

        if (cols != getCols()) {
            for (auto & line : _active) {
                line.cells.resize(cols, Cell::blank());
            }
        }

        if (rows > getRows()) {
            _active.resize(rows, ALine(cols));
        }
        else if (rows < getRows()) {
            _active.erase(_active.begin() + rows, _active.end());
        }

        _cols = cols;

        ASSERT(getRows() == rows && getCols() == cols, "");

        _active.shrink_to_fit();

        resetMargins();

        _tabs.resize(cols);
        resetTabs();

        _cursor.pos.row = std::min<int16_t>(_cursor.pos.row, rows - 1);
        _cursor.pos.col = std::min<int16_t>(_cursor.pos.col, cols - 1);
        _cursor.wrapNext = false;

        _savedCursor.pos.row = std::min<int16_t>(_savedCursor.pos.row, rows - 1);
        _savedCursor.pos.col = std::min<int16_t>(_savedCursor.pos.col, cols - 1);

        _damage.resize(rows);
        damageViewport(false);
    }

    void resizeReflow(int16_t rows, int16_t cols) {
        ASSERT(rows > 0 && cols > 0, "");
        ASSERT(_cursor.pos.row >= 0 && _cursor.pos.row < getRows(), "");
        ASSERT(_cursor.pos.col >= 0 && _cursor.pos.col < getCols(), "");

        clearSelection();

        // Remove blank lines from the back, stopping if we hit the cursor.
        while (getRows() > rows &&
               _cursor.pos.row < getRows() - 1 &&
               _active.back().isBlank())
        {
            _active.pop_back();
        }

        if (cols != getCols()) {
            bool     doneCursor     = false;
            uint32_t cursorTagIndex = 0;
            uint32_t cursorOffset   = 0;

            _active.back().cont = false;            // JUST MAKE SURE it isn't on for now.
            ASSERT(!_active.back().cont, "");           // XXX this needs enforcing, e.g. in insertLineAt

            while (!_active.empty()) {
                auto s = _pending.size();

                bump();

                if (!doneCursor) {
                    if (_cursor.pos.row == 0) {
                        cursorTagIndex = _tags.size() - 1;
                        cursorOffset   = s + _cursor.pos.col;
                        doneCursor     = true;
                    }
                    else {
                        --_cursor.pos.row;
                    }
                }
            }

            if (_cursor.wrapNext) {
                ++cursorOffset;
                _cursor.wrapNext = false;
            }

            ASSERT(doneCursor, "");
            ASSERT(!_tags.empty(), "");
            ASSERT(_pending.empty(), "");

            _cols = cols;       // Must set before calling rebuildHistory()
            rebuildHistory();

            doneCursor = false;

            // Pull rows out of history first.
            while (getRows() < rows && !_history.empty()) {
                if (doneCursor) {
                    ++_cursor.pos.row;
                }
                else if (_tags.size() - 1 == cursorTagIndex) {
                    auto & hline = _history.back();
                    uint32_t offset = hline.seqnum * cols;
                    if (cursorOffset == offset + cols) {
                        _cursor.pos.row  = 0;
                        _cursor.pos.col  = cursorOffset - offset;
                        _cursor.wrapNext = true;
                        doneCursor       = true;
                    }
                    else if (cursorOffset < offset + cols) {
                        _cursor.pos.row = 0;
                        _cursor.pos.col = cursorOffset - offset;
                        doneCursor      = true;

                        if (_cursor.pos.col < 0) {
                            // This can happen due to libreadline stepping on our toes.
                            _cursor.pos.col = 0;
                        }
                    }
                }
                unbump();
            }

            ASSERT(_cursor.pos.col >= 0, "");

            if (!doneCursor) {
                _cursor.pos.row = 0;
                _cursor.pos.col = 0;
            }

            // Add blank lines to get the rest.
            if (getRows() < rows) {
                _active.resize(rows, ALine(cols));
            }

            ASSERT(_active.size() == static_cast<size_t>(rows), "");
            ASSERT(_active.front().cells.size() == static_cast<size_t>(cols), "");
        }
        else {
            if (getRows() < rows) {
                // Pull rows out of history first.
                while (getRows() < rows && !_history.empty()) {
                    ++_cursor.pos.row;
                    unbump();
                }

                // Add blank lines to get the rest.
                if (getRows() < rows) {
                    _active.resize(rows, ALine(cols));
                }
            }
            else if (getRows() > rows) {
                // Push excess rows into history.
                while (getRows() > rows) {
                    if (_cursor.pos.row > 0) { --_cursor.pos.row; }
                    bump();
                }
            }
        }

        ASSERT(getRows() == rows && getCols() == cols, "rows=" << getRows() << ", cols=" << getCols());
        ASSERT(_cursor.pos.row >= 0, "");
        ASSERT(_cursor.pos.col >= 0, "");

        _active.shrink_to_fit();

        _scrollOffset = std::min<uint32_t>(_scrollOffset, _history.size());

        resetMargins();

        _tabs.resize(cols);
        resetTabs();

        _cursor.pos.row = std::min<int16_t>(_cursor.pos.row, rows - 1);
        _cursor.pos.col = std::min<int16_t>(_cursor.pos.col, cols - 1);

        _savedCursor.pos.row = std::min<int16_t>(_savedCursor.pos.row, rows - 1);
        _savedCursor.pos.col = std::min<int16_t>(_savedCursor.pos.col, cols - 1);

        _damage.resize(rows);
        damageViewport(true);
    }

    void tabCursor(TabDir dir, uint16_t count) {
        auto col = _cursor.pos.col;

        switch (dir) {
            case TabDir::FORWARD:
                while (count != 0) {
                    ++col;

                    if (col == getCols()) {
                        --col;
                        break;
                    }

                    if (_tabs[col]) {
                        --count;
                    }
                }
                break;
            case TabDir::BACKWARD:
                while (count != 0) {
                    if (col == 0) {
                        break;
                    }

                    --col;

                    if (_tabs[col]) {
                        --count;
                    }
                }
                break;
        }

        moveCursor2(true, 0, false, col);
    }

    void reset() {
        clear();
        resetMargins();
        resetTabs();
        resetCursor();
    }

    void setMargins(int16_t begin, int16_t end) {
        if (end > begin) {
            _marginBegin = clamp<int16_t>(begin, 0, getRows() - 1);
            _marginEnd   = clamp<int16_t>(end,   begin + 1, getRows());
        }
        else {
            resetMargins();
        }
    }

    void resetMargins() {
        _marginBegin = 0;
        _marginEnd   = getRows();
    }

    void resetTabs() {
        for (size_t i = 0; i != _tabs.size(); ++i) {
            _tabs[i] = i % 8 == 0;
        }
    }

    void resetCursor() {
        _cursor.pos      = Pos();
        _cursor.wrapNext = false;
        // XXX should _cursor.charSet be reset?
        resetStyle();
    }

    void resetStyle() {
        _cursor.style = Style();
    }

    void setAttr(Attr attr) {
        _cursor.style.attrs.set(attr);
    }

    void unsetAttr(Attr attr) {
        _cursor.style.attrs.unset(attr);
    }

    void setFg(const UColor & color) {
        _cursor.style.fg = color;
    }

    void setBg(const UColor & color) {
        _cursor.style.bg = color;
    }

    void insertCells(uint16_t n) {
        APos begin, end;
        if (normaliseSelection(begin, end)) {
            if (begin.row <= _cursor.pos.row && end.row >= _cursor.pos.row) {
                clearSelection();
            }
        }

        n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);

        auto & line = _active[_cursor.pos.row];
        line.wrap = std::min<int16_t>(getCols(), line.wrap + n);
        ASSERT(line.wrap <= getCols(), "");
        std::copy_backward(line.cells.begin() + _cursor.pos.col,
                           line.cells.end() - n,
                           line.cells.end());
        std::fill(line.cells.begin() + _cursor.pos.col,
                  line.cells.begin() + _cursor.pos.col + n,
                  Cell::blank());

        damageColumns(_cursor.pos.col, getCols());

        ASSERT(!line.cont || line.wrap == _cols,
               "line.cont=" << std::boolalpha << line.cont <<
               ", line.wrap=" << line.wrap <<
               ", _cols=" << _cols);

        _cursor.wrapNext = false;
    }

    void eraseCells(uint16_t n) {
        APos begin, end;
        if (normaliseSelection(begin, end)) {
            if (begin.row <= _cursor.pos.row && end.row >= _cursor.pos.row) {
                clearSelection();
            }
        }

        n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);

        auto & line = _active[_cursor.pos.row];
        line.cont = false;
        line.wrap = std::max<int16_t>(0, line.wrap - n);
        ASSERT(line.wrap <= getCols(), "");
        std::copy(line.cells.begin() + _cursor.pos.col + n,
                  line.cells.end(),
                  line.cells.begin() + _cursor.pos.col);
        std::fill(line.cells.end() - n,
                  line.cells.end(),
                  Cell::blank());

        damageColumns(_cursor.pos.col, getCols());

        ASSERT(!line.cont || line.wrap == _cols,
               "line.cont=" << std::boolalpha << line.cont <<
               ", line.wrap=" << line.wrap <<
               ", _cols=" << _cols);

        _cursor.wrapNext = false;
    }

    void blankCells(uint16_t n) {
        _cursor.wrapNext = false;
        n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);

        auto & line = _active[_cursor.pos.row];

        for (uint16_t i = 0; i != n; ++i) {
            line.cells[_cursor.pos.col + i] = Cell::ascii(SPACE, _cursor.style);
        }

        damageColumns(_cursor.pos.col, _cursor.pos.col + n);
    }

    void clearLine() {
        auto & line = _active[_cursor.pos.row];

        line.cont = false;
        line.wrap = 0;
        _cursor.wrapNext = false;
        std::fill(line.cells.begin(), line.cells.end(), Cell::blank());
        damageColumns(0, getCols());

        ASSERT(!line.cont || line.wrap == _cols,
               "line.cont=" << std::boolalpha << line.cont <<
               ", line.wrap=" << line.wrap <<
               ", _cols=" << _cols);

        _cursor.wrapNext = false;
    }

    void clearLineLeft() {
        auto & line = _active[_cursor.pos.row];

        _cursor.wrapNext = false;
        std::fill(line.cells.begin(),
                  line.cells.begin() + _cursor.pos.col + 1,
                  Cell::blank());
        damageColumns(0, _cursor.pos.col + 1);

        ASSERT(!line.cont || line.wrap == _cols,
               "line.cont=" << std::boolalpha << line.cont <<
               ", line.wrap=" << line.wrap <<
               ", _cols=" << _cols);

        _cursor.wrapNext = false;
    }

    void clearLineRight() {
        auto & line = _active[_cursor.pos.row];

        line.cont = false;
        _cursor.wrapNext = false;
        line.wrap = std::min(line.wrap, _cursor.pos.col);
        ASSERT(line.wrap <= getCols(), "");
        std::fill(line.cells.begin() + _cursor.pos.col, line.cells.end(), Cell::blank());
        damageColumns(_cursor.pos.col, line.cells.size());

        ASSERT(!line.cont || line.wrap == _cols,
               "line.cont=" << std::boolalpha << line.cont <<
               ", line.wrap=" << line.wrap <<
               ", _cols=" << _cols);

        _cursor.wrapNext = false;
    }

    void clear() {
        for (auto & l : _active) { l.clear(); }
        damageActive();
        _cursor.wrapNext = false;
    }

    void clearAbove() {
        clearLineLeft();
        for (auto i = _active.begin(); i != _active.begin() + _cursor.pos.row; ++i) {
            i->clear();
        }
        clearSelection();
    }

    void clearBelow() {
        clearLineRight();
        for (auto i = _active.begin() + _cursor.pos.row + 1; i != _active.end(); ++i) {
            i->clear();
        }
        clearSelection();
    }

    void insertLines(uint16_t n) {
        if (_cursor.pos.row >= _marginBegin && _cursor.pos.row < _marginEnd) {
            _cursor.wrapNext = false;
            n = std::min<uint16_t>(n, _marginEnd - _cursor.pos.row);
            insertLinesAt(_cursor.pos.row, n);
        }
    }

    void eraseLines(uint16_t n) {
        if (_cursor.pos.row >= _marginBegin && _cursor.pos.row < _marginEnd) {
            _cursor.wrapNext = false;
            n = std::min<uint16_t>(n, _marginEnd - _cursor.pos.row);
            eraseLinesAt(_cursor.pos.row, n);
        }
    }

    void scrollUpMargins(uint16_t n) {
        _cursor.wrapNext = false;
        n = std::min<uint16_t>(n, _marginEnd - _marginBegin);
        eraseLinesAt(_marginBegin, n);
    }

    void scrollDownMargins(uint16_t n) {
        _cursor.wrapNext = false;
        n = std::min<uint16_t>(n, _marginEnd - _marginBegin);
        insertLinesAt(_marginBegin, n);
    }

    void damageViewport(bool scrollbar) {
        for (auto & d : _damage) {
            d.damageSet(0, getCols());
        }

        if (scrollbar) {
            _barDamage = true;
        }
    }

    void damageActive() {
        damageRows(0, getRows());
    }

    void testPattern() {
        for (auto & r : _active) {
            for (auto & c : r.cells) {
                c = Cell::ascii('E', _cursor.style);
            }
        }
        damageActive();
    }

    void accumulateDamage(int16_t & rowBegin, int16_t & rowEnd,
                          int16_t & colBegin, int16_t & colEnd) const {
        bool first = true;
        int16_t rowNum = 0;

        for (auto & d : _damage) {
            auto cB = d.begin;
            auto cE = d.end;

            if (cB != cE) {
                if (first) {
                    rowBegin = rowNum;
                    rowEnd   = rowNum + 1;
                    colBegin = cB;
                    colEnd   = cE;
                    first    = false;
                }
                else {
                    rowEnd   = rowNum + 1;
                    colBegin = std::min(colBegin, cB);
                    colEnd   = std::max(colEnd,   cE);
                }
            }

            ++rowNum;
        }
    }

    template <class Func> void dispatchBg(bool reverse, Func func) const {
        APos selBegin, selEnd;
        bool selValid = normaliseSelection(selBegin, selEnd);

        for (int16_t r = 0; r != static_cast<int16_t>(_active.size()); ++r) {
            auto & d = _damage[r];
            if (d.begin == d.end) { continue; }

            const std::vector<Cell> * cellsPtr;
            uint32_t                  offset;
            int16_t                   wrap;

            if (static_cast<uint32_t>(r) < _scrollOffset) {
                auto & hline = _history[_history.size() - _scrollOffset + r];
                auto   tag   = _tags[hline.index - _lostTags];

                cellsPtr = tag == I_Deduper::invalidTag() ? &_pending : &_deduper.lookup(tag);
                offset   = hline.seqnum * getCols();
                wrap     = cellsPtr->size() - offset;
            }
            else {
                auto & aline = _active[r - _scrollOffset];

                cellsPtr = &aline.cells;
                offset   = 0;
                wrap     = aline.wrap;
            }

            auto & cells = *cellsPtr;
            auto   blank = Cell::blank();
            auto   bg0   = UColor::stock(UColor::Name::TEXT_BG);
            auto   c0    = d.begin;  // Accumulation start column.
            auto   c1    = c0;

            for (; c1 != d.end; ++c1) {
                // Once we get past the wrap point all cells should be the same, so skip
                // to the last iteration. Unlike in dispatchFg() we must iterate over
                // the wrap character to handle selection correctly.
                if (c1 > wrap) { c1 = d.end - 1; }

                auto   apos     = APos(r - _scrollOffset, c1);
                auto   selected = selValid && isCellSelected(apos, selBegin, selEnd, wrap);
                auto   c        = c1 + offset;
                auto & cell     = c < cells.size() ? cells[c] : blank;
                auto & attrs    = cell.style.attrs;
                auto   swap     = XOR(reverse, attrs.get(Attr::INVERSE));
                auto   bg1      = bg0; // About to be overridden.

                if (selected) {
                    if (_config.customSelectBgColor) {
                        bg1 = UColor::stock(UColor::Name::SELECT_BG);
                    }
                    else if (_config.customSelectFgColor) {
                        bg1 = swap ? cell.style.fg : cell.style.bg;
                    }
                    else {
                        bg1 = !swap ? cell.style.fg : cell.style.bg;
                    }
                }
                else {
                    bg1 = swap ? cell.style.fg : cell.style.bg;
                }

                if (bg0 != bg1) {
                    if (c1 != c0) {
                        func(Pos(r, c0), c1 - c0, bg0);
                    }

                    c0  = c1;
                    bg0 = bg1;
                }
            }

            // There may be an unterminated run to flush.
            if (c1 != c0) {
                func(Pos(r, c0), c1 - c0, bg0);
            }
        }
    }

    template <class Func> void dispatchFg(bool reverse, Func func) const {
        APos selBegin, selEnd;
        bool selValid = normaliseSelection(selBegin, selEnd);

        std::vector<uint8_t> run;         // Buffer for accumulating character runs.

        for (int16_t r = 0; r != static_cast<int16_t>(_active.size()); ++r) {
            auto & d = _damage[r];
            if (d.begin == d.end) { continue; }

            const std::vector<Cell> * cellsPtr;
            uint32_t                  offset;
            int16_t                   wrap;

            if (static_cast<uint32_t>(r) < _scrollOffset) {
                auto & hline = _history[_history.size() - _scrollOffset + r];
                auto   tag   = _tags[hline.index - _lostTags];

                cellsPtr = tag == I_Deduper::invalidTag() ? &_pending : &_deduper.lookup(tag);
                offset   = hline.seqnum * getCols();
                wrap     = cellsPtr->size() - offset;
            }
            else {
                auto & aline = _active[r - _scrollOffset];

                cellsPtr = &aline.cells;
                offset   = 0;
                wrap     = aline.wrap;
            }

            auto & cells  = *cellsPtr;
            auto   fg0    = UColor::stock(UColor::Name::TEXT_FG);
            auto   attrs0 = AttrSet();
            auto   c0     = d.begin;   // Accumulation start column.
            auto   c1     = c0;

            for (; c1 != d.end; ++c1) {
                // Once we get past the wrap point all cells will be blank,
                // so break out of the loop now.
                if (c1 >= wrap) { break; }

                auto   apos     = APos(r - _scrollOffset, c1);
                auto   selected = selValid && isCellSelected(apos, selBegin, selEnd, wrap);
                auto   c        = c1 + offset;
                ASSERT(c < cells.size(), "");
                auto & cell     = cells[c];
                auto & attrs1   = cell.style.attrs;
                auto   swap     = XOR(reverse, attrs1.get(Attr::INVERSE));
                auto   fg1      = fg0; // About to be overridden.

                if (selected) {
                    if (_config.customSelectFgColor) {
                        fg1 = UColor::stock(UColor::Name::SELECT_FG);
                    }
                    else if (_config.customSelectBgColor) {
                        fg1 = swap ? cell.style.bg : cell.style.fg;
                    }
                    else {
                        fg1 = !swap ? cell.style.bg : cell.style.fg;
                    }
                }
                else {
                    fg1 = swap ? cell.style.bg : cell.style.fg;
                }

                if (fg0 != fg1 || attrs0 != attrs1) {
                    if (c1 != c0) {
                        // flush run
                        auto size = run.size();
                        run.push_back(NUL);
                        func(Pos(r, c0), c1 - c0, fg0, attrs0, &run.front(), size);
                        run.clear();
                    }

                    c0     = c1;
                    fg0    = fg1;
                    attrs0 = attrs1;
                }

                utf8::Length length = utf8::leadLength(cell.seq.lead());
                std::copy(cell.seq.bytes, cell.seq.bytes + length, std::back_inserter(run));
            }

            // There may be an unterminated run to flush.
            if (c1 != c0) {
                // flush run
                auto size = run.size();
                run.push_back(NUL);
                func(Pos(r, c0), c1 - c0, fg0, attrs0, &run.front(), size);
                run.clear();
            }
        }
    }

    template <class Func> void dispatchCursor(bool reverse, Func func) const {
        APos selBegin, selEnd;
        bool selValid = normaliseSelection(selBegin, selEnd);

        int32_t r0 = _cursor.pos.row + getScrollOffset();
        if (r0 >= 0 && r0 < getRows()) {
            auto   r1       = static_cast<int16_t>(r0);
            auto   c1       = _cursor.pos.col;
            auto & aline    = _active[r1];
            auto   wrap     = aline.wrap;
            auto   apos     = APos(r1 - _scrollOffset, c1);
            auto   selected = selValid && isCellSelected(apos, selBegin, selEnd, wrap);
            auto & cell     = aline.cells[c1];
            auto & attrs    = cell.style.attrs;
            auto   swap     = XOR(reverse, attrs.get(Attr::INVERSE));
            auto   fg       = cell.style.fg;
            auto   bg       = cell.style.bg;
            if (XOR(selected, swap)) { std::swap(fg, bg); }

            if (_config.customCursorFillColor) {
                fg = UColor::stock(UColor::Name::CURSOR_FILL);
            }

            if (_config.customCursorTextColor) {
                bg = UColor::stock(UColor::Name::CURSOR_TEXT);
            }

            auto length = utf8::leadLength(cell.seq.lead());
            std::vector<uint8_t> run;         // Buffer for accumulating character runs.
            std::copy(cell.seq.bytes, cell.seq.bytes + length, std::back_inserter(run));

            auto size = run.size();
            run.push_back(NUL);
            func(Pos(r1, c1), fg, bg, attrs, &run.front(), size, _cursor.wrapNext);
        }
    }

    void useCharSet(CharSet charSet) {
        _cursor.charSet = charSet;
    }

    void setCharSub(CharSet charSet, const CharSub * charSub) {
        _cursor.setCharSub(charSet, charSub);
    }

    void dumpTags(std::ostream & ost) const {
        ost << "BEGIN LOCAL TAGS" << std::endl;

        size_t i = 0;

        for (auto t : _tags) {
            ost << std::setw(6) << i << " "
                << std::setw(sizeof(I_Deduper::Tag) * 2) << std::setfill('0')
                << std::hex << std::uppercase << t << ": "
                << std::setfill(' ') << std::dec << " \'";

            auto & cells = t != I_Deduper::invalidTag() ? _deduper.lookup(t) : _pending;

            for (auto & c : cells) {
                ost << c.seq;
            }

            ost << "\'" << std::endl;

            ++i;
        }

        ost << "END LOCAL TAGS" << std::endl << std::endl;
    }

    void dumpHistory(std::ostream & ost) const {
        ost << "BEGIN HISTORY" << std::endl;

        size_t i = 0;

        for (const auto & l : _history) {
            ost << std::setw(4) << i << " "
                << std::setw(4) << l.index - _lostTags << " "
                << std::setw(2) << l.seqnum << " "
                << std::setw(3) << l.size << " \'";

            auto   tag   = _tags[l.index - _lostTags];
            auto & cells = tag != I_Deduper::invalidTag() ? _deduper.lookup(tag) : _pending;

            size_t offset = l.seqnum * getCols();
            auto   blank  = Cell::blank();
            for (uint16_t o = 0; o != l.size; ++o) {
                auto c = offset + o;
                const auto & cell = c < cells.size() ? cells[c] : blank;
                ost << cell.seq;
            }

            ost << "\'" << std::endl;

            ++i;
        }

        ost << "END HISTORY" << std::endl << std::endl;
    }

    void dumpActive(std::ostream & ost) const {
        ost << "BEGIN ACTIVE" << std::endl;

        size_t i = 0;

        for (const auto & l : _active) {
            ost << std::setw(2) << i << " "
                << (l.cont ? '\\' : '$') << " "
                << std::setw(3) << l.wrap << " \'";

            uint16_t col = 0;

            ost << SGR::UNDERLINE;
            for (; col != l.wrap; ++col) { ost << l.cells[col].seq; }
            ost << SGR::RESET_UNDERLINE;

            for (; col != getCols(); ++col) { ost << l.cells[col].seq; }

            ost << "\'" << std::endl;

            ++i;
        }

        ost << "END ACTIVE" << std::endl << std::endl;
    }

    void dumpSelection(std::ostream & ost) const {
        ost << "BEGIN SELECTION" << std::endl;

        std::string text;
        if (getSelectedText(text)) {
            ost << text << std::endl;
        }

        ost << "END SELECTION" << std::endl << std::endl;
    }

protected:
    void rebuildHistory() {
        _history.clear();

        uint32_t index = 0;

        for (auto tag : _tags) {
            auto & cells = tag != I_Deduper::invalidTag() ? _deduper.lookup(tag) : _pending;

            uint16_t seqnum = 0;
            size_t   offset = 0;

            do {
                auto size = std::min<int16_t>(_cols, cells.size() - offset);
                //PRINT("offset=" << offset << ", seqnum=" << seqnum << ", size=" << size);
                _history.push_back(HLine(index + _lostTags, seqnum, size));

                ENFORCE(seqnum != static_cast<uint16_t>(-1), "Very long line: overflow");
                ++seqnum;
                offset += _cols;
            } while (offset < cells.size());

            ++index;
        }
    }

    static bool isCellSelected(APos apos, APos begin, APos end, int16_t wrap) {
        if (apos.row >= begin.row && apos.row <= end.row) {
            // apos is within the selected row range
            if (apos.row == begin.row && (apos.col < begin.col || begin.col >= wrap)) {
                return false;
            }
            else if (apos.row == end.row && apos.col >= end.col) {
                // apos is right of the selection end
                return end.col > wrap &&
                    (apos.row > begin.row || (apos.row == begin.row && wrap > begin.col));
            }
            else {
                return true;
            }
        }
        else {
            return false;
        }
    }

    bool normaliseSelection(APos & begin, APos & end) const {
        auto b = _selectMark;
        auto e = _selectDelim;

        if (b == e) {
            return false;
        }

        if (e < b) { std::swap(b, e); }

        if (b.hand == Hand::LEFT || b.apos.col >= _cols - 1) {
            begin = b.apos;
        }
        else {
            begin = APos(b.apos.row, b.apos.col + 1);
        }

        if (e.hand == Hand::RIGHT || e.apos.col == 0) {
            end = e.apos;
        }
        else {
            end = APos(e.apos.row, e.apos.col - 1);
        }

        if (end.col < _cols) {
            ++end.col;
        }

        return begin.row != end.row || begin.col != end.col;
    }

    void insertLinesAt(int16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
               ", margin-end=" << _marginEnd);

        APos begin, end;
        if (normaliseSelection(begin, end)) {
            if (begin.row < row && end.row >= row) {
                // Clear the selection because it spans the insertion point.
                clearSelection();
            }
            else if (begin.row < _marginEnd - n && end.row >= _marginEnd - n) {
                // Clear the selection because it spans or will collide with the
                // erasure point.
                clearSelection();
            }
            else {
                _selectMark.apos.row  += n;
                _selectDelim.apos.row += n;
            }
        }

        _active.erase (_active.begin() + _marginEnd - n, _active.begin() + _marginEnd);
        _active.insert(_active.begin() + row, n, ALine(getCols()));

        damageRows(row, _marginEnd);
    }

    void eraseLinesAt(int16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
               ", margin-end=" << _marginEnd);

        APos begin, end;
        if (normaliseSelection(begin, end)) {
            if (begin.row < row + n && end.row >= row) {
                // Clear the selection because it spans the erasure point or will collidate
                // with the erasure point.
                clearSelection();
            }
            else if (begin.row < _marginEnd && end.row >= _marginEnd) {
                // Clear the selection because it spans the insertion point.
                clearSelection();
            }
            else {
                _selectMark.apos.row  -= n;
                _selectDelim.apos.row -= n;
            }
        }


        _active.erase (_active.begin() + row, _active.begin() + row + n);
        _active.insert(_active.begin() + _marginEnd - n, n, ALine(getCols()));

        damageRows(row, _marginEnd);
    }

    bool marginsSet() const {
        return _marginBegin != 0 || _marginEnd != getRows();
    }

    void damageCell() {
        auto damageRow = _scrollOffset + static_cast<uint32_t>(_cursor.pos.row);

        if (damageRow < static_cast<uint32_t>(getRows())) {
            _damage[damageRow].damageAdd(_cursor.pos.col, _cursor.pos.col + 1);
        }
    }

    void damageColumns(int16_t begin, int16_t end) {
        ASSERT(begin <= end, "");
        ASSERT(begin >= 0, "");
        ASSERT(end   <= getCols(), "");

        auto damageRow = _scrollOffset + static_cast<uint32_t>(_cursor.pos.row);

        if (damageRow < static_cast<uint32_t>(getRows())) {
            _damage[damageRow].damageAdd(begin, end);
        }
    }

    void damageRows(int16_t begin, int16_t end) {
        ASSERT(begin <= end, "");

        for (auto i = begin; i != end; ++i) {
            auto damageRow = _scrollOffset + static_cast<uint32_t>(i);

            if (damageRow < static_cast<uint32_t>(getRows())) {
                _damage[i].damageSet(0, getCols());
            }
            else {
                break;
            }
        }
    }

    void damageSelection() {
        APos begin, end;

        if (normaliseSelection(begin, end)) {
            // Convert to viewport relative.
            auto row0 = begin.row   + static_cast<int32_t>(_scrollOffset);
            auto row1 = end.row + 1 + static_cast<int32_t>(_scrollOffset);

            // Clamp to viewport.
            auto row2 = std::max<int32_t>(row0, 0);
            auto row3 = std::min<int32_t>(row1, getRows());

            if (row2 < row3) {
                for (auto i = row2; i != row3; ++i) {
                    _damage[i].damageSet(0, getCols());
                }
            }
        }
    }

    void addLine() {
        if (marginsSet()) {
            eraseLinesAt(_marginBegin, 1);
        }
        else {
            if (_historyLimit == 0) {
                _active.pop_front();
            }
            else {
                bump();

                if (!_config.scrollWithHistory) {
                    if (_scrollOffset != 0 && _scrollOffset != _history.size()) {
                        ++_scrollOffset;
                    }
                }

                enforceHistoryLimit();
            }

            _active.push_back(ALine(getCols()));

            APos begin, end;
            if (normaliseSelection(begin, end)) {
                if (begin.row == -static_cast<int32_t>(_history.size())) {
                    clearSelection();
                }
                else {
                    --_selectMark.apos.row;
                    --_selectDelim.apos.row;
                }
            }

            damageViewport(true);
        }
    }

    void bump() {
        auto & aline = _active.front();

        ASSERT(!aline.cont || aline.wrap == _cols,
               "aline.cont=" << std::boolalpha << aline.cont <<
               ", aline.wrap=" << aline.wrap <<
               ", _cols=" << _cols);

        if (_pending.empty()) {
            // This line is not a continuation of a previous line.
            ASSERT(_tags.empty() || _tags.back() != I_Deduper::invalidTag(), "");
            ASSERT(_history.empty() || _history.back().index - _lostTags == _tags.size() - 1, "");

            if (aline.cont) {
                // This line is continued on the next line so it can't be stored
                // for dedupe yet.
                _pending = std::move(aline.cells);
                _tags.push_back(I_Deduper::invalidTag());
            }
            else {
                // This line is completely standalone. Immediately dedupe it.
                ASSERT(static_cast<size_t>(aline.wrap) <= aline.cells.size(), "");
                aline.cells.erase(aline.cells.begin() + aline.wrap, aline.cells.end());
                auto tag = _deduper.store(aline.cells);
                ASSERT(tag != I_Deduper::invalidTag(), "");
                _tags.push_back(tag);
            }

            _history.push_back(HLine(_tags.size() + _lostTags - 1, 0, aline.wrap));
        }
        else {
            // This line is a continuation of the previous line.
            // Copy its contents into _pending.
            ASSERT(!_tags.empty(), "");
            ASSERT(_tags.back() == I_Deduper::invalidTag(), "");
            ASSERT(!_history.empty(), "");
            ASSERT(_history.back().index - _lostTags == _tags.size() - 1, "");
            auto oldSize = _pending.size();
            ASSERT(oldSize % _cols == 0, "");
            _pending.resize(oldSize + aline.wrap, Cell::blank());
            std::copy(aline.cells.begin(), aline.cells.begin() + aline.wrap, _pending.begin() + oldSize);
            _history.push_back(HLine(_tags.size() + _lostTags - 1, _history.back().seqnum + 1, aline.wrap));

            if (!aline.cont) {
                // This line is not itself continued.
                // Store _pending and the tag.
                auto tag = _deduper.store(_pending);
                ASSERT(tag != I_Deduper::invalidTag(), "");
                ASSERT(_pending.empty(), "_pending cleared by I_Deduper::store()");
                ASSERT(_tags.back() == I_Deduper::invalidTag(), "");
                _tags.back() = tag;
            }
        }

        ASSERT(!_history.empty() && _history.back().index - _lostTags == _tags.size() - 1, "");

        _active.pop_front();
    }

    void unbump() {
        ASSERT(!_tags.empty(), "");
        ASSERT(!_history.empty(), "");

        auto & hline = _history.back();
        ASSERT(hline.index - _lostTags == _tags.size() - 1, "");

        bool cont;

        if (_pending.empty()) {
            cont = false;
            auto tag = _tags.back();
            ASSERT(tag != I_Deduper::invalidTag(), "");
            _pending     = _deduper.lookupRemove(tag);
            _tags.back() = I_Deduper::invalidTag();
        }
        else {
            cont = true;
            ASSERT(_tags.back() == I_Deduper::invalidTag(), "");
        }

        size_t offset = hline.seqnum * _cols;
        ASSERT(offset <= _pending.size(), "");
        std::vector<Cell> cells(_pending.begin() + offset, _pending.end());
        _pending.erase(_pending.begin() + offset, _pending.end());

        _active.push_front(ALine(cells, cont, cells.size(), _cols));
        ASSERT(_active.front().wrap <= _cols, "");

        _history.pop_back();

        if (_history.empty() || _history.back().index - _lostTags != _tags.size() - 1) {
            _tags.pop_back();
            ASSERT(_pending.empty(), "");
        }
    }

    void enforceHistoryLimit() {
        while (_tags.size() > _historyLimit) {
            while (!_history.empty() && _history.front().index == _lostTags) {
                if (_scrollOffset == _history.size()) { --_scrollOffset; }
                _history.pop_front();
            }

            _deduper.remove(_tags.front());
            _tags.pop_front();
            ++_lostTags;
        }
    }
};

#endif // COMMON__BUFFER__HXX
