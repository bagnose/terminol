// vi:noai:sw=4

#ifndef COMMON__BUFFER__HXX
#define COMMON__BUFFER__HXX

#include "terminol/common/data_types.hxx"
#include "terminol/common/config.hxx"
#include "terminol/common/deduper_interface.hxx"
#include "terminol/support/escape.hxx"

#include <deque>
#include <vector>
#include <iomanip>

class CharSub {
    const utf8::Seq * _seqs;
    size_t            _offset;
    size_t            _size;
    bool              _special;

public:
    CharSub() : _seqs(nullptr), _offset(0), _size(0), _special(false) {}

    CharSub(const utf8::Seq * seqs, size_t offset, size_t size, bool special = false) :
        _seqs(seqs), _offset(offset), _size(size), _special(special) {}

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

enum class CharSet {
    G0, G1
};

enum class TabDir { FORWARD, BACKWARD };

//
//
//

class Buffer {
    struct APos {
        int32_t row;
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

    struct HAPos {
        APos apos;
        Hand hand;

        HAPos() : apos(), hand(Hand::LEFT) {}
        HAPos(HPos hpos, uint32_t offset) : apos(hpos.pos, offset), hand(hpos.hand) {}
        HAPos(int32_t row_, int16_t col_) : apos(row_, col_), hand(Hand::LEFT) {}
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
        uint32_t index;
        uint16_t seqnum;
        uint16_t size;

        HLine(uint32_t index_,
              uint16_t seqnum_,
              uint16_t size_) :
            index(index_), seqnum(seqnum_), size(size_) {}
    };

    // Active-Line
    struct ALine {
        std::vector<Cell> cells;
        bool              cont;     // Continuation from a 'wrap-next'?
        int16_t           wrap;     // Wrappable index.

        explicit ALine(int16_t cols) :
            cells(cols, Cell::blank()), cont(false), wrap(0) {}

        ALine(std::vector<Cell> & cells_, bool cont_, int16_t wrap_, int16_t cols) :
            cells(std::move(cells_)), cont(cont_), wrap(wrap_)
        {
            cells.resize(cols, Cell::blank());
        }

        void clear() {
            wrap = 0; cont = false;
            std::fill(cells.begin(), cells.end(), Cell::blank());
        }

        bool isBlank() const {
            for (const auto & c : cells) {
                if (c != Cell::blank()) { return false; }
            }
            return true;
        }
    };

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

    const Config               & _config;
    I_Deduper                  & _deduper;
    std::deque<I_Deduper::Tag>   _tags;
    std::vector<Cell>            _pending;
    std::deque<HLine>            _history;
    std::deque<ALine>            _active;
    std::vector<Damage>          _damage;           // *viewport* relative damage
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

    struct Cursor {
        Pos             pos;
        Style           style;
        bool            wrapNext;

        CharSet         cs;
        const CharSub * g0;
        const CharSub * g1;

        Cursor(const CharSub * g0_, const CharSub * g1_) :
            pos(), style(), wrapNext(false), cs(CharSet::G0), g0(g0_), g1(g1_) {}
    };

    Cursor             _cursor;
    Cursor             _savedCursor;

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

    uint32_t getHistory() const { return _history.size(); }
    uint32_t getTotal() const { return _history.size() + _active.size(); }
    uint32_t getBar() const { return _history.size() - _scrollOffset; }
    uint32_t getScrollOffset() const { return _scrollOffset; }
    bool     getBarDamage() const { return _barDamage; }

    void markSelection(HPos hpos) {
        damageSelection();
        _selectMark = _selectDelim = HAPos(hpos, _scrollOffset);
    }

    void delimitSelection(HPos hpos, bool fresh) {
        damageSelection();

        HAPos hapos(hpos, _scrollOffset);

        if (fresh) {
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

            HAPos centre(row, col);

            if (hapos < centre) {
                std::swap(_selectMark, _selectDelim);
            }
        }

        _selectDelim = hapos;
        damageSelection();
    }

    void expandSelection(HPos UNUSED(pos), int UNUSED(level)) {
    }

    void clearSelection() {
        damageSelection();
        _selectDelim = _selectMark;     // XXX need to be careful about this not pointing to valid data
    }

    bool getSelectedText(std::string & text) const {
        APos begin, end;

        if (normaliseSelection(begin, end)) {
            for (auto i = begin; i.row <= end.row; ++i.row, i.col = 0) {
                /*
                if (i.row != begin.row) {
                    text.push_back('\n');
                }
                */

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

        if (_scrollOffset + rows > getHistory()) {
            _scrollOffset = getHistory();
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
        if (_scrollOffset != getHistory()) {
            _scrollOffset = getHistory();
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

        auto cs = _cursor.cs == CharSet::G0 ? _cursor.g0 : _cursor.g1;
        cs->translate(seq);

        if (autoWrap && _cursor.wrapNext) {
            _cursor.wrapNext = false;

            if (_cursor.pos.row == _marginEnd - 1) {
                addLine();
                moveCursor2(true, 0, false, 0);
            }
            else {
                moveCursor2(true, 1, false, 0);
            }

            _active[_cursor.pos.row].cont = true; // continues from previous line
        }
        else if (insert) {
            insertCells(1);
        }

        auto style = _cursor.style;
        if (cs->isSpecial()) {
            style.attrs.unset(Attr::BOLD);
            style.attrs.unset(Attr::ITALIC);
        }
        auto & line = _active[_cursor.pos.row];
        line.cells[_cursor.pos.col] = Cell::utf8(seq, style);
        if (autoWrap) {
            line.wrap = std::max<int16_t>(line.wrap, _cursor.pos.col + 1);
        }
        else {
            line.wrap = 0;
            line.cont = false;
        }

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
    }

    void restoreCursor() {
        damageCell();
        _cursor = _savedCursor;
        damageCell();
    }

    void resizeClip(int16_t rows, int16_t cols) {
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

        _damage.resize(rows);
        damageViewport(false);
    }

    void resizeReflow(int16_t rows, int16_t cols) {
        /*
        std::cerr
            << "resizeReflow: "
            << getRows() << "x" << getCols()
            << " --> "
            << rows << "x" << cols
            << std::endl;
            */

        if (getRows() > rows) {
            // Remove blank lines from the back.
            while (getRows() > rows && _active.back().isBlank()) {
                _active.pop_back();
            }
        }

        if (cols != getCols()) {
            while (getRows() != 0) {
                bump();
            }

            //dumpHistory(std::cerr);

            // This block is copied from bump().
            if (!_tags.empty() && _tags.back() == I_Deduper::invalidTag()) {
                auto & hline = _history.back();
                ASSERT(hline.index - _lostTags == _tags.size() - 1, "");
                size_t finalSize = hline.seqnum * getCols() + hline.size;
                ASSERT(finalSize <= _pending.size(), "");
                _pending.erase(_pending.begin() + finalSize, _pending.end());
                // Following 3 lines probably aren't required.
#if 0
                auto tag = _deduper.store(_pending);
                ASSERT(tag != I_Deduper::invalidTag(), "");
                _tags.back() = tag;
#endif
            }

            _history.clear();

            uint32_t index = 0;

            for (auto tag : _tags) {
#if 0
                ASSERT(tag != I_Deduper::invalidTag(), "");    // This is prevented above
#endif
                auto & cells = tag != I_Deduper::invalidTag() ? _deduper.lookup(tag) : _pending;

                uint16_t seqnum = 0;
                size_t   offset = 0;

                do {
                    auto size = std::min<int16_t>(cols, cells.size() - offset);
                    //PRINT("offset=" << offset << ", seqnum=" << seqnum << ", size=" << size);
                    _history.push_back(HLine(index + _lostTags, seqnum, size));

                    ++seqnum;
                    offset += cols;
                } while (offset < cells.size());

                ++index;
            }

            _cols = cols;

            //dumpHistory(std::cerr);
        }

        if (getRows() < rows) {
            // Pull rows out of history first.
            while (getRows() < rows && !_history.empty()) {
                unbump();
            }

            // Add blank lines to get the rest.
            if (getRows() < rows) {
                _active.resize(rows, ALine(cols));
            }
        }
        else if (getRows() > rows) {
            // And push the rest into history.
            while (getRows() > rows) {
                bump();
            }
        }

        //dumpActive(std::cerr);

        ASSERT(getRows() == rows && getCols() == cols, "rows=" << getRows() << ", cols=" << getCols());

        _active.shrink_to_fit();

        _scrollOffset = std::min<uint32_t>(_scrollOffset, _history.size());

        resetMargins();

        _tabs.resize(cols);
        resetTabs();

        _cursor.pos.row = std::min<int16_t>(_cursor.pos.row, rows - 1);
        _cursor.pos.col = std::min<int16_t>(_cursor.pos.col, cols - 1);

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
        _cursor.pos = Pos();
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
        n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);

        auto & line = _active[_cursor.pos.row];
        std::copy_backward(line.cells.begin() + _cursor.pos.col,
                           line.cells.end() - n,
                           line.cells.end());
        std::fill(line.cells.begin() + _cursor.pos.col,
                  line.cells.begin() + _cursor.pos.col + n,
                  Cell::blank());

        damageColumns(_cursor.pos.col, getCols());
    }

    void eraseCells(uint16_t n) {
        n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);

        auto & line = _active[_cursor.pos.row];
        std::copy(line.cells.begin() + _cursor.pos.col + n,
                  line.cells.end(),
                  line.cells.begin() + _cursor.pos.col);
        std::fill(line.cells.end() - n,
                  line.cells.end(),
                  Cell::blank());

        damageColumns(_cursor.pos.col, getCols());
    }

    void blankCells(uint16_t n) {
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
        std::fill(line.cells.begin(), line.cells.end(), Cell::blank());
        damageColumns(0, getCols());
    }

    void clearLineLeft() {
        auto & line = _active[_cursor.pos.row];

        line.cont = false;
        line.wrap = 0;
        std::fill(line.cells.begin(),
                  line.cells.begin() + _cursor.pos.col + 1,
                  Cell::blank());
        damageColumns(0, _cursor.pos.col + 1);
    }

    void clearLineRight() {
        auto & line = _active[_cursor.pos.row];

        line.wrap = std::min(line.wrap, _cursor.pos.col);
        std::fill(line.cells.begin() + _cursor.pos.col, line.cells.end(), Cell::blank());
        damageColumns(_cursor.pos.col, line.cells.size());
    }

    void clear() {
        for (auto & l : _active) { l.clear(); }
        damageActive();
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
            n = std::min<uint16_t>(n, _marginEnd - _cursor.pos.row);
            insertLinesAt(_cursor.pos.row, n);
        }
    }

    void eraseLines(uint16_t n) {
        if (_cursor.pos.row >= _marginBegin && _cursor.pos.row < _marginEnd) {
            n = std::min<uint16_t>(n, _marginEnd - _cursor.pos.row);
            eraseLinesAt(_cursor.pos.row, n);
        }
    }

    void scrollUpMargins(uint16_t n) {
        n = std::min<uint16_t>(n, _marginEnd - _marginBegin);
        eraseLinesAt(_marginBegin, n);
    }

    void scrollDownMargins(uint16_t n) {
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

    template <class Func>
    void dispatchBg(bool reverse, Func func) const {
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
                auto & aline = _active[r - _scrollOffset];

                cellsPtr = &aline.cells;
                offset   = 0;
                wrap     = aline.wrap;
            }

            auto & cells = *cellsPtr;
            auto   blank = Cell::blank();

            auto bg0 = UColor::stock(UColor::Name::TEXT_BG);
            auto c0  = d.begin;  // Accumulation start column.
            auto c1  = c0;

            for (; c1 != d.end; ++c1) {
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
                        func(Pos(r, c0), bg0, c1 - c0);
                    }

                    c0  = c1;
                    bg0 = bg1;
                }
            }

            // There may be an unterminated run to flush.
            if (c1 != c0) {
                func(Pos(r, c0), bg0, c1 - c0);
            }
        }
    }

    template <class Func>
    void dispatchFg(bool reverse, Func func) const {
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
                auto & aline = _active[r - _scrollOffset];

                cellsPtr = &aline.cells;
                offset   = 0;
                wrap     = aline.wrap;
            }

            auto & cells = *cellsPtr;
            auto   blank = Cell::blank();

            auto fg0    = UColor::stock(UColor::Name::TEXT_FG);
            auto attrs0 = AttrSet();
            auto c0     = d.begin;   // Accumulation start column.
            auto c1     = c0;

            for (; c1 != d.end; ++c1) {
                auto   apos     = APos(r - _scrollOffset, c1);
                auto   selected = selValid && isCellSelected(apos, selBegin, selEnd, wrap);
                auto   c        = c1 + offset;
                auto & cell     = c < cells.size() ? cells[c] : blank;
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
                        func(Pos(r, c0), fg0, attrs0, &run.front(), size, c1 - c0);
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
                func(Pos(r, c0), fg0, attrs0, &run.front(), size, c1 - c0);
                run.clear();
            }
        }
    }

    template <class Func>
    void dispatchCursor(bool reverse, Func func) const {
        APos selBegin, selEnd;
        bool selValid = normaliseSelection(selBegin, selEnd);

        int32_t r0 = _cursor.pos.row + getScrollOffset();
        if (r0 >= 0 && r0 < getRows()) {
            auto r1 = static_cast<int16_t>(r0);
            auto c1 = _cursor.pos.col;

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

    void useCharSet(CharSet cs) {
        _cursor.cs = cs;
    }

    void setCharSet(CharSet cs, const CharSub * sub) {
        switch (cs) {
            case CharSet::G0:
                _cursor.g0 = sub;
                break;
            case CharSet::G1:
                _cursor.g1 = sub;
                break;
        }
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

            auto tag = _tags[l.index - _lostTags];
            auto & cells = tag != I_Deduper::invalidTag() ? _deduper.lookup(tag) : _pending;

            size_t offset = l.seqnum * getCols();
            const Cell blank = Cell::blank();
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
                << (l.cont ? '+' : '-') << " "
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

        //PRINT("Normalised: " << begin.row << "x" << begin.col << "  " << end.row << "x" << end.col);

        return begin.row != end.row || begin.col != end.col;
    }

    void insertLinesAt(int16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
               ", margin-end=" << _marginEnd);

        _active.erase (_active.begin() + _marginEnd - n, _active.begin() + _marginEnd);
        _active.insert(_active.begin() + row, n, ALine(getCols()));

        damageRows(row, _marginEnd);

        clearSelection();
    }

    void eraseLinesAt(int16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
               ", margin-end=" << _marginEnd);

        _active.erase (_active.begin() + row, _active.begin() + row + n);
        _active.insert(_active.begin() + _marginEnd - n, n, ALine(getCols()));

        damageRows(row, _marginEnd);

        clearSelection();
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
        ASSERT(end <= _marginEnd, "");

        for (auto i = begin; i != end; ++i) {
            auto damageRow = _scrollOffset + static_cast<uint32_t>(i);

            if (damageRow < static_cast<uint32_t>(getRows())) {
                _damage[i].damageSet(0, getCols());
            }
        }
    }

    void damageSelection() {
        damageViewport(false);        // FIXME just damage selection
    }

    void addLine() {
        if (marginsSet()) {
            _active.erase (_active.begin() + _marginBegin);
            _active.insert(_active.begin() + _marginEnd - 1, ALine(getCols()));

            damageRows(_marginBegin, _marginEnd);
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
            }

            _active.push_back(ALine(getCols()));

            damageViewport(true);
        }

        --_selectMark.apos.row;
        --_selectDelim.apos.row;

        enforceHistoryLimit();
    }

    void bump() {
        //std::cerr << "bump" << std::endl;

        auto & aline = _active.front();

        if (aline.cont && !_tags.empty()) {
            ASSERT(_tags.back() == I_Deduper::invalidTag(), "");
            ASSERT(!_history.empty(), "");
            ASSERT(_history.back().index - _lostTags == _tags.size() - 1, "");
            auto oldSize = _pending.size();
            ASSERT(oldSize % _cols == 0, "");
            _pending.resize(oldSize + _cols, Cell::blank());
            std::copy(aline.cells.begin(), aline.cells.begin() + aline.wrap, _pending.begin() + oldSize);
            _history.push_back(HLine(_tags.size() + _lostTags - 1, _history.back().seqnum + 1, aline.wrap));
        }
        else {
            if (!_tags.empty() && _tags.back() == I_Deduper::invalidTag()) {
                auto & hline = _history.back();
                ASSERT(hline.index - _lostTags == _tags.size() - 1, "");
                size_t finalSize = hline.seqnum * getCols() + hline.size;
                ASSERT(finalSize <= _pending.size(), "");
                _pending.erase(_pending.begin() + finalSize, _pending.end());
                auto tag = _deduper.store(_pending);
                ASSERT(tag != I_Deduper::invalidTag(), "");
                _tags.back() = tag;
            }

            ASSERT(_tags.empty() || _tags.back() != I_Deduper::invalidTag(), "");
            ASSERT(_history.empty() || _history.back().index - _lostTags == _tags.size() - 1, "");

            _pending = std::move(aline.cells);
            _tags.push_back(I_Deduper::invalidTag());
            _history.push_back(HLine(_tags.size() + _lostTags - 1, 0, aline.wrap));
        }

        ASSERT(!_tags.empty() && _tags.back() == I_Deduper::invalidTag(), "");
        ASSERT(!_history.empty() && _history.back().index - _lostTags == _tags.size() - 1, "");

        _active.pop_front();
    }

    void unbump() {
        //std::cerr << "un-bump" << std::endl;

        ASSERT(!_tags.empty(), "");
        ASSERT(!_history.empty(), "");

        auto & hline = _history.back();
        ASSERT(hline.index - _lostTags == _tags.size() - 1, "");

        if (_tags.back() != I_Deduper::invalidTag()) {
            auto tag = _tags.back();
            _deduper.lookupRemove(tag, _pending);
            _tags.back() = I_Deduper::invalidTag();
        }

        size_t offset = hline.seqnum * _cols;
        std::vector<Cell> cells(_pending.begin() + offset, _pending.end());
        _pending.erase(_pending.begin() + offset, _pending.end());

        //PRINT(hline.seqnum);
        _active.push_front(ALine(cells, hline.seqnum != 0, hline.size, _cols));

        _history.pop_back();

        if (_history.empty() || _history.back().index - _lostTags != _tags.size() - 1) {
            _tags.pop_back();
            _pending.clear();
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
