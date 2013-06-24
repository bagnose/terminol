// vi:noai:sw=4

#ifndef COMMON__BUFFER__HXX
#define COMMON__BUFFER__HXX

#include "terminol/common/data_types.hxx"
#include "terminol/common/config.hxx"
#include "terminol/support/escape.hxx"

#include <unordered_map>
#include <deque>
#include <vector>
#include <iomanip>

#include <cstddef>
#include <stdint.h>

namespace {

template <class T> struct SDBM {
    typedef T Type;
    T operator () (T val, uint8_t next) {
        return static_cast<T>(next) + (val << 6) + (val << 16) - val;
    }
};

template <class A> typename A::Type hash(const void * buffer,
                                         size_t       length) {
    auto buf = static_cast<const uint8_t *>(buffer);
    return std::accumulate(buf, buf + length, typename A::Type(0), A());
}

} // namespace {anonymous}

//
//
//

class Deduper {
public:
    // In reality it's a choice between 32-bit and 64-bit tags.

    //typedef uint64_t Tag;
    typedef uint32_t Tag;
    //typedef uint16_t Tag;
    //typedef uint8_t Tag;

    struct Line {
        Line() : cont(false), wrap(0), cells() {}

        Line(bool cont_, uint16_t wrap_, std::vector<Cell> & cells_) :
            cont(cont_),
            wrap(wrap_),
            cells(std::move(cells_)) {}

        bool              cont;
        uint16_t          wrap;
        std::vector<Cell> cells;
    };

private:
    struct Payload {
        Payload(bool cont_, uint16_t wrap_, std::vector<Cell> & cells_) :
            line(cont_, wrap_, cells_),
            refCount(1) {}

        Line     line;
        uint32_t refCount;
    };

    std::unordered_map<Tag, Payload> _lines;
    size_t                           _totalRefCount;

public:
    Deduper() : _lines(), _totalRefCount(0) {}

    Tag store(bool cont, uint16_t wrap, std::vector<Cell> & cells) {
        auto tag = makeTag(cells);

again:
        auto iter = _lines.find(tag);

        if (iter == _lines.end()) {
            _lines.insert(std::make_pair(tag, Payload(cont, wrap, cells)));
        }
        else {
            auto & payload = iter->second;

            if (cont  != payload.line.cont ||
                wrap  != payload.line.wrap ||
                cells != payload.line.cells) {
#if DEBUG
                std::cerr << "Collision between:" << std::endl;
                std::cerr << "   '";
                for (auto c : cells) { std::cerr << c.seq; }
                std::cerr << "'" << std::endl;
                std::cerr << "And:" << std::endl;
                std::cerr << "   '";
                for (auto c : payload.line.cells) { std::cerr << c.seq; }
                std::cerr << "'" << std::endl << std::endl;
#endif

                ENFORCE(static_cast<Tag>(_lines.size()) != 0, "No dedupe room left");

                ++tag;
                goto again;
            }

            ++payload.refCount;
        }

        ++_totalRefCount;

#if 0
        if (_totalRefCount != 0) {
            std::cerr << "+++ " << 100.0 * double(_lines.size()) / double(_totalRefCount) << " %" << std::endl;
        }
#endif

        return tag;
    }

    const Line & lookup(Tag tag) const {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        return iter->second.line;
    }

    void remove(Tag tag) {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & payload = iter->second;

        if (--payload.refCount == 0) {
            _lines.erase(iter);
        }

        --_totalRefCount;

#if 0
        if (_totalRefCount != 0) {
            std::cerr << "--- " << 100.0 * double(_lines.size()) / double(_totalRefCount) << " %" << std:endl;
        }
#endif
    }

    void lookupRemove(Tag tag, Line & line) {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & payload = iter->second;

        if (--payload.refCount == 0) {
            line.cont  = payload.line.cont;
            line.wrap  = payload.line.wrap;
            line.cells = std::move(payload.line.cells);
            ASSERT(payload.line.cells.empty(), "");
            _lines.erase(iter);
        }
        else {
            line = payload.line;
        }
    }

private:
    Tag makeTag(const std::vector<Cell> & cells) const {
        return hash<SDBM<Tag>>(&cells.front(), sizeof(Cell) * cells.size());
    }
};

//
//
//

class Buffer {
    class Line {
        bool              _cont;        // Continuation from a 'wrap-next'
        uint16_t          _wrap;        // Wrappable index
        std::vector<Cell> _cells;
        //
        uint16_t          _cols;        // May be more cells than this due to resize.
        uint16_t          _damageBegin;
        uint16_t          _damageEnd;

        std::vector<Cell>::iterator begin() { return _cells.begin(); }
        std::vector<Cell>::iterator end()   { return _cells.begin() + _cols; }

    public:
        std::vector<Cell>::const_iterator begin() const { return _cells.begin(); }
        std::vector<Cell>::const_iterator end()   const { return _cells.begin() + _cols; }

        explicit Line(uint16_t cols, bool continuation = false) :
            _cont(continuation),
            _wrap(0),
            _cells(cols, Cell::blank()),
            _cols(cols),
            _damageBegin(0),
            _damageEnd(cols) {}

        Line(uint16_t cols, bool cont, uint16_t wrap, std::vector<Cell> & cells_) :
            _cont(cont),
            _wrap(wrap),
            _cells(std::move(cells_)),
            _cols(cols),
            _damageBegin(0),
            _damageEnd(cols)
        {
            resizePreserve(cols);
            ASSERT(_wrap <= _cols, "");
        }

        const std::vector<Cell> & cells() const { return _cells; }

        const Cell & getCell(uint16_t col) const {
            ASSERT(col < _cols, "");
            return _cells[col];
        }

        void getDamage(uint16_t & colBegin, uint16_t & colEnd) const {
            colBegin = _damageBegin;
            colEnd   = _damageEnd;
        }

        void insert(uint16_t col, uint16_t n) {
            trim();

            ASSERT(col + n <= _cells.size(), "");

            std::copy_backward(begin() + col, end() - n, end());
            std::fill(begin() + col, begin() + col + n, Cell::blank());

            damageAdd(col, _cells.size());
        }

        void erase(uint16_t col, uint16_t n) {
            trim();

            ASSERT(col + n <= _cells.size(), "");

            std::copy(begin() + col + n, end(), begin() + col);
            std::fill(end() - n, end(), Cell::blank());

            damageAdd(col, _cells.size());
        }

        void setContinuation() {
            _cont = true;
        }

        bool isContinuation() const {
            return _cont;
        }

        uint16_t getWrap() const { return _wrap; }

        bool setCell(uint16_t col, const Cell & cell, bool autoWrap) {
            trim();

            ASSERT(col < _cells.size(), "");

            bool seqChanged = _cells[col].seq != cell.seq;
            _cells[col] = cell;

            if (autoWrap) { _wrap = col + 1; }
            else          { _wrap = 0; _cont = false; }

            damageAdd(col, col + 1);

            return seqChanged;
        }

        void resizeClip(uint16_t cols) {
            uint16_t oldCols = _cells.size();
            _cells.resize(cols, Cell::blank());
            if (oldCols < cols) {
                damageAdd(oldCols, cols);
            }

            _cols = cols;
        }

        void resizePreserve(uint16_t cols) {
            if (_cells.size() < cols) {
                uint16_t oldCols = _cells.size();
                _cells.resize(cols, Cell::blank());
                damageAdd(oldCols, cols);
            }

            _cols = cols;
            _wrap = std::min(_wrap, _cols);
        }

        void clear() {
            trim();

            _wrap = 0; _cont = false;
            std::fill(begin(), end(), Cell::blank());
            damageAll();
        }

        void clearLeft(uint16_t endCol) {
            trim();

            _wrap = 0; _cont = false;
            std::fill(begin(), begin() + endCol, Cell::blank());
            damageAdd(0, endCol);
        }

        void clearRight(uint16_t beginCol) {
            trim();

            _wrap = 0; _cont = false;
            std::fill(begin() + beginCol, end(), Cell::blank());
            damageAdd(beginCol, _cells.size());
        }

        bool isBlank() const {
            for (const auto & c : *this) {
                if (c != Cell::blank()) { return false; }
            }
            return true;
        }

        void resetDamage() {
            _damageBegin = _damageEnd = 0;
        }

        void damageAll() {
            _damageBegin = 0;
            _damageEnd   = _cells.size();
        }

        void damageAdd(uint16_t begin_, uint16_t end_) {
            ASSERT(begin_ <= end_, "");
            ASSERT(end_   <= _cells.size(), "");

            if (_damageBegin == _damageEnd) {
                // No damage yet.
                _damageBegin = begin_;
                _damageEnd   = end_;
            }
            else {
                _damageBegin = std::min(_damageBegin, begin_);
                _damageEnd   = std::max(_damageEnd,   end_);
            }
        }

        void trim() {
            if (_cells.size() != _cols) {
                ASSERT(_cells.size() > _cols, "");
                _cells.erase(begin() + _cols, end());
            }
        }

        void wrapFrom(Line & previous, uint16_t n) {
            ASSERT(previous._wrap >= n, "");
            ASSERT(n != 0, "");

            _cells.insert(begin(),
                          previous.begin() + previous.getWrap() - n,
                          previous.begin() + previous.getWrap());
            std::fill(previous.begin() + previous.getWrap() - n,
                      previous.begin() + previous.getWrap(),
                      Cell::blank());
            _wrap += n;
            previous._wrap -= n;

            damageAll();
        }

        void unwrapTo(Line & previous, uint16_t n) {
            ASSERT(n <= _cols, "");
            ASSERT(n != 0, "");

            previous._cells.insert(previous.begin() + previous.getWrap(),
                                   begin(),
                                   begin() + n);
            std::copy(begin() + n, end(), begin());
            std::fill(end() - n, end(), Cell::blank());

            previous._wrap += n;
            _wrap -= n;

            damageAll();
        }
    };

    //
    //
    //

    struct APos {
        APos() : row(0), col(0) {}
        APos(uint32_t row_, uint16_t col_) : row(row_), col(col_) {}
        APos(Pos pos, uint32_t offset) : row(pos.row + offset), col(pos.col) {}

        uint32_t row;
        uint16_t col;

        friend bool operator == (APos lhs, APos rhs) {
            return lhs.row == rhs.row && lhs.col == rhs.col;
        }

        friend bool operator != (APos lhs, APos rhs) {
            return !(lhs == rhs);
        }
    };

    static const Cell BLANK;

    const Config             & _config;
    Deduper                  & _deduper;

    std::deque<Deduper::Tag>   _history;
    std::deque<Line>           _active;
    uint16_t                   _cols;

    size_t                     _historyLimit;
    size_t                     _scrollOffset;

    uint16_t                   _marginBegin;
    uint16_t                   _marginEnd;

    bool                       _barDamage;

    int                        _selectExpansion;
    APos                       _selectMarker;
    APos                       _selectDelimiter;

public:
    Buffer(const Config & config,
           Deduper      & deduper,
           uint16_t       rows,
           uint16_t       cols,
           size_t         historyLimit) :
        _config(config),
        _deduper(deduper),
        _history(),
        _active(rows, Line(cols)),
        _cols(cols),
        _historyLimit(historyLimit),
        _scrollOffset(0),
        _marginBegin(0),
        _marginEnd(rows),
        _barDamage(true),
        _selectExpansion(0),
        _selectMarker(),
        _selectDelimiter()
    {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");
    }

    ~Buffer() {
        for (auto tag : _history) {
            _deduper.remove(tag);
        }
    }

    size_t   getHistory() const { return _history.size(); }
    uint16_t getRows()    const { return _active.size(); }
    uint16_t getCols()    const { return _cols; }
    size_t   getBar()     const { return _history.size() - _scrollOffset; }
    size_t   getTotal()   const { return getHistory() + getRows(); }

    size_t   getScrollOffset() const { return _scrollOffset; }

    uint16_t getMarginBegin() const { return _marginBegin; }
    uint16_t getMarginEnd()   const { return _marginEnd;   }
    uint16_t getMarginSize()  const { return _marginEnd - _marginBegin; }

    bool     getBarDamage()   const { return _barDamage; }

    bool scrollUpHistory(uint16_t rows) {
        auto oldScrollOffset = _scrollOffset;

        if (_scrollOffset + rows > getHistory()) {
            _scrollOffset = getHistory();
        }
        else {
            _scrollOffset += rows;
        }

        return _scrollOffset != oldScrollOffset;
    }

    bool scrollDownHistory(uint16_t rows) {
        auto oldScrollOffset = _scrollOffset;

        if (rows > _scrollOffset) {
            _scrollOffset = 0;
        }
        else {
            _scrollOffset -= rows;
        }

        return _scrollOffset != oldScrollOffset;
    }

    bool scrollTopHistory() {
        if (_scrollOffset != getHistory()) {
            _scrollOffset = getHistory();
            return true;
        }
        else {
            return false;
        }
    }

    bool scrollBottomHistory() {
        if (_scrollOffset != 0) {
            _scrollOffset = 0;
            return true;
        }
        else {
            return false;
        }
    }

    void setMargins(uint16_t begin_, uint16_t end_) {
        ASSERT(begin_ < end_, "");
        ASSERT(end_ <= getRows(), "");
        _marginBegin = begin_;
        _marginEnd   = end_;
    }

    void resetMargins() {
        _marginBegin = 0;
        _marginEnd   = getRows();
    }

protected:
    bool marginsSet() const {
        return _marginBegin != 0 || _marginEnd != getRows();
    }

    void bump() {
        ASSERT(_historyLimit != 0, "");

        auto cont  = _active.front().isContinuation();
        auto wrap  = _active.front().getWrap();
        auto cells = std::move(_active.front().cells());
        _active.pop_front();
        while (!cells.empty() && cells.back() == Cell::blank()) {
            cells.pop_back();
        }
        cells.shrink_to_fit();
        auto tag = _deduper.store(cont, wrap, cells);

        if (_history.size() == _historyLimit) {
            _deduper.remove(_history.front());
            _history.pop_front();
        }

        _history.push_back(tag);
    }

    void unbump() {
        ASSERT(!_history.empty(), "");

        auto tag = _history.back();
        Deduper::Line line;
        _deduper.lookupRemove(tag, line);
        _active.push_front(Line(_cols, line.cont, line.wrap, line.cells));
        _history.pop_back();
    }

    void normalSelection(APos & b, APos & e) const {
        b = _selectMarker;
        e = _selectDelimiter;

        // Re-order aBegin and aEnd if necessary.
        if (b.row > e.row || (b.row == e.row && b.col > e.col)) {
            std::swap(b, e);
        }

        if (b.col == _cols) {
            ++b.row;
            b.col = 0;
        }

        if (e.col == 0) {
            --e.row;
            e.col = _cols;
        }
    }

public:

    //
    // Render / scroll-relative operations.
    //

    const Cell & getCell(Pos pos) const {
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col < getCols(), "");

        if (pos.row < _scrollOffset) {
            auto tag = _history[_history.size() - _scrollOffset + pos.row];
            const auto & line = _deduper.lookup(tag);
            return pos.col < line.cells.size() ? line.cells[pos.col] : BLANK;
        }
        else {
            return _active[pos.row - _scrollOffset].getCell(pos.col);
        }
    }

    void getDamage(uint16_t row, uint16_t & colBegin, uint16_t & colEnd) const {
        ASSERT(row < getRows(), "");

        if (row < _scrollOffset) {
            // XXX maybe this is an error
            colBegin = colEnd = 0;
        }
        else {
            _active[row - _scrollOffset].getDamage(colBegin, colEnd);

            // The line may be wider than the buffer because we don't
            // shrink it on resize.
            colBegin = std::min(_cols, colBegin);
            colEnd   = std::min(_cols, colEnd);
        }
    }

    bool getContinuationAbs(uint32_t row) const {
        if (row < _history.size()) {
            auto tag = _history[row];
            const auto & line = _deduper.lookup(tag);
            return line.cont;
        }
        else {
            uint16_t r = row - _history.size();
            ASSERT(r < getRows(), r << " < " << getRows());
            return _active[r].isContinuation();
        }
    }

    const Cell & getCellAbs(APos pos) const {
        if (pos.row < _history.size()) {
            auto tag = _history[pos.row];
            const auto & line = _deduper.lookup(tag);
            return pos.col < line.cells.size() ? line.cells[pos.col] : BLANK;
        }
        else {
            uint16_t r = pos.row - _history.size();
            ASSERT(r < getRows(), r << " < " << getRows());
            return _active[r].getCell(pos.col);
        }
    }

    bool getSelectedText(std::string & text) const {
        // If the marker and delimiter are the same then the selection is empty.
        if (_selectMarker == _selectDelimiter) {
            return false;
        }

        APos b, e;
        normalSelection(b, e);

        for (auto i = b; i.row <= e.row; ++i.row, i.col = 0) {
            if (!getContinuationAbs(i.row)) {
                if (!text.empty()) { text.push_back('\n'); }
            }

            auto lastNonBlank = text.size();

            for (; i.col < getCols() && (i.row < e.row || i.col != e.col); ++i.col)
            {
                const auto & cell = getCellAbs(i);
                auto         seq  = cell.seq;
                std::copy(&seq.bytes[0],
                          &seq.bytes[utf8::leadLength(seq.lead())],
                          back_inserter(text));

                if (seq != Cell::blank().seq) {
                    lastNonBlank = text.size();
                }
            }

            text.erase(text.begin() + lastNonBlank, text.end());
        }

        return true;
    }

    bool getSelectedArea(Pos & begin, Pos & end, bool & topless, bool & bottomless) const {
        // If the marker and delimiter are the same then the selection is empty.
        if (_selectMarker == _selectDelimiter) {
            return false;
        }

        APos b, e;
        normalSelection(b, e);

        uint32_t topOffset = _history.size() - _scrollOffset;

        // Test if the selection intersects the visible region.
        if (b.row < topOffset + getRows() && e.row >= topOffset)
        {
            if (b.row < topOffset) {
                topless = true;
                begin = Pos();
            }
            else {
                topless = false;
                begin = Pos(b.row - topOffset, b.col);
            }

            if (e.row >= topOffset + getRows()) {
                bottomless = true;
                end = Pos(getRows(), getCols());
            }
            else{
                bottomless = false;
                end = Pos(e.row - topOffset, e.col);
            }

            return true;
        }
        else {
            return false;
        }
    }

    //
    // Select / scroll-relative operations.
    //

    void markSelection(Pos pos) {
        _selectExpansion = 0;
        _selectMarker    = APos(pos, _history.size() - _scrollOffset);
        _selectDelimiter = _selectMarker;
    }

    void delimitSelection(Pos pos) {
        _selectDelimiter = APos(pos, _history.size() - _scrollOffset);
    }

    void adjustSelection(Pos pos) {
        if (_selectMarker == _selectDelimiter) { return; }

        APos b, e;
        normalSelection(b, e);

        auto p = APos(pos, _history.size());

        if (p.row < b.row || (p.row == b.row && p.col < b.col)) {
            b = p;
        }
        else if (p.row > e.row || (p.row == e.row && p.col > e.col)) {
            e = p;
        }

        _selectMarker    = b;
        _selectDelimiter = e;
    }

    void expandSelection(Pos UNUSED(pos)) {
        APos b, e;
        normalSelection(b, e);

        // TODO
    }

    void clearSelection() {
        if (_selectMarker != _selectDelimiter) {
            _selectMarker    = APos();
            _selectDelimiter = _selectMarker;

            // FIXME this doesn't work for historical
            damageAll();
        }

        _selectExpansion = 0;
    }

    //
    // Update / active-relative operations.
    //

    void insertCells(Pos pos, uint16_t n) {
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col <= getCols(), "");
        ASSERT(pos.col + n <= getCols(), "");
        _active[pos.row].insert(pos.col, n);
    }

    void eraseCells(Pos pos, uint16_t n) {
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col < getCols(), "");
        _active[pos.row].erase(pos.col, n);
    }

    void setContinuation(uint16_t row) {
        ASSERT(row < getRows(), "");
        _active[row].setContinuation();
    }

    void setCells(Pos pos, uint16_t n, const Cell & cell) {
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col < getCols(), "");
        ASSERT(pos.col + n <= getCols(), "");

        for (uint16_t i = 0; i != n; ++i) {
            setCell(pos.right(i), cell, false);
        }
    }

    void setCell(Pos pos, const Cell & cell, bool autoWrap) {
        ASSERT(cell.seq.lead() != NUL, "");
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col < getCols(), "");

        if (_active[pos.row].setCell(pos.col, cell, autoWrap)) {
            if (_selectMarker != _selectDelimiter) {
                // There is a selection
                APos b, e;
                normalSelection(b, e);

                APos apos(pos, _history.size() - _scrollOffset);

                if (apos.row >= b.row && apos.row <= e.row) {
                    clearSelection();
                }
            }
        }
    }

    void resizeClip(uint16_t rows, uint16_t cols, Pos & cursor) {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");

        if (cols != _cols) {
            for (auto & line : _active) {
                line.resizeClip(cols);
            }

            if (cols <= cursor.col) {
                cursor.col = cols - 1;
            }

            _cols = cols;
        }

        if (rows > getRows()) {
            _active.resize(rows, Line(_cols));
        }
        else if (rows < getRows()) {
            _active.erase(_active.begin() + rows, _active.end());

            if (rows <= cursor.row) {
                cursor.row = rows - 1;
            }
        }

        _active.shrink_to_fit();

        resetMargins();
        clearSelection();
        damageAll();

        ASSERT(getRows() == rows && getCols() == cols, "");
    }

    void resizePreserve(uint16_t rows, uint16_t cols, Pos & cursor) {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");

        //
        // Cols
        //

        if (cols != _cols) {
            for (auto & line : _active) {
                line.resizePreserve(cols);
            }

            if (cols <= cursor.col) {
                cursor.col = cols - 1;
            }

            _cols = cols;
        }

        //
        // Rows
        //

        if (rows > getRows()) {
            // The scrollback history is the first port of call for rows.

            while (!_history.empty() && getRows() < rows) {
                unbump();
                ++cursor.row;
            }

            // And resize the container to obtain the rest.

            _active.resize(rows, Line(_cols));
        }
        else if (rows < getRows()) {
            // Remove blanks lines from the back.
            while (getRows() != rows) {
                if (_active.back().isBlank() && cursor.row != _active.size() - 1) {
                    _active.pop_back();
                }
                else {
                    break;
                }
            }

            if (getRows() - rows < cursor.row) {
                cursor.row -= getRows() - rows;
            }
            else {
                cursor.row = 0;
            }

            if (_historyLimit == 0) {
                // Optimisation, just erase lines if they aren't going into history.
                _active.erase(_active.begin(), _active.begin() + getRows() - rows);
            }
            else {
                // Move lines into history.
                while (getRows() != rows) {
                    bump();
                }
            }
        }

        //
        // Consolidation
        //

        if (_scrollOffset != 0) {
            if (_config.scrollOnResize) {
                // Scroll to bottom
                _scrollOffset = 0;
            }
            else {
                // Preserve the top line
                //_scrollOffset = _scrollOffset - delta;        // XXX
                ASSERT(!(_scrollOffset > _history.size()), "");
            }
        }

        _active.shrink_to_fit();

        resetMargins();
        clearSelection();
        damageAll();

        ASSERT(getRows() == rows && getCols() == cols, "");

        // XXX temporary measures to avoid crash
        if (cursor.row >= getRows()) { cursor.row = getRows() - 1; }
        if (cursor.col >= getCols()) { cursor.col = getCols() - 1; }
    }

    void resizeReflow(uint16_t rows, uint16_t cols, Pos & cursor) {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");

        for (size_t i = 0; i != _config.reflowHistory && !_history.empty(); ++i) {
            unbump();
            ++cursor.row;
        }

        //
        // Cols
        //

        if (cols > _cols) {
            //const uint16_t deltaCols = cols - _cols;

            uint16_t availability;
            uint16_t r = 0;

            while (r != _active.size()) {
                _active[r].resizeClip(cols);
                ASSERT(cols >= _active[r].getWrap(), "");

                if (r != 0 && _active[r].isContinuation()) {
                    if (availability < _active[r].getWrap()) {
                        // Partial consumption.
                        if (availability > 0) {
                            _active[r].unwrapTo(_active[r - 1], availability);
                        }
                        availability = cols - _active[r].getWrap();
                        ++r;
                    }
                    else {
                        // Complete consumption - erase line.
                        _active[r].unwrapTo(_active[r - 1], _active[r].getWrap());
                        availability = cols - _active[r - 1].getWrap();
                        _active.erase(_active.begin() + r);
                        if (cursor.row >= r) { --cursor.row; }
                    }
                }
                else {
                    availability = cols - _active[r].getWrap();
                    ++r;
                }
            }

            _cols = cols;
        }
        else if (cols < _cols) {
            uint16_t r = 0;

            while (r != _active.size()) {
                if (_active[r].getWrap() > cols) {
                    // The current line has wrappable content that would otherwise
                    // be clipped.

                    uint16_t excess = _active[r].getWrap() - cols;

                    if (r + 1 == _active.size() || !_active[r + 1].isContinuation()) {
                        _active.insert(_active.begin() + r + 1, Line(0, true));
                        if (cursor.row >= r + 1) { ++cursor.row; }
                    }

                    _active[r + 1].wrapFrom(_active[r], excess);
                }

                _active[r].resizeClip(cols);
                ++r;
            }

            if (cols <= cursor.col) {           // XXX Not good enough
                cursor.col = cols - 1;
            }

            _cols = cols;
        }

        //
        // Rows
        //

        if (rows > getRows()) {
            // The scrollback history is the first port of call for rows.

            while (!_history.empty() && getRows() < rows) {
                unbump();
                ++cursor.row;
            }

            // And resize the container to obtain the rest.

            _active.resize(rows, Line(_cols));
        }
        else if (rows < getRows()) {
            // Remove blanks lines from the back.
            while (getRows() != rows) {
                if (_active.back().isBlank() && cursor.row != _active.size() - 1) {
                    _active.pop_back();
                }
                else {
                    break;
                }
            }

            if (getRows() - rows < cursor.row) {
                cursor.row -= getRows() - rows;
            }
            else {
                cursor.row = 0;
            }

            if (_historyLimit == 0) {
                // Optimisation, just erase lines if they aren't going into history.
                _active.erase(_active.begin(), _active.begin() + getRows() - rows);
            }
            else {
                // Move lines into history.
                while (getRows() != rows) {
                    bump();
                }
            }
        }

        //
        // Consolidation
        //

        if (_scrollOffset != 0) {
            if (_config.scrollOnResize) {
                // Scroll to bottom
                _scrollOffset = 0;
            }
            else {
                // Preserve the top line
                //_scrollOffset = _scrollOffset - delta;        // XXX
                ASSERT(!(_scrollOffset > _history.size()), "");
            }
        }

        _active.shrink_to_fit();

        resetMargins();
        clearSelection();
        damageAll();

        ASSERT(getRows() == rows && getCols() == cols, "");

        // XXX temporary measures to avoid crash
        if (cursor.row >= getRows()) { cursor.row = getRows() - 1; }
        if (cursor.col >= getCols()) { cursor.col = getCols() - 1; }
    }

    void addLine() {
        if (marginsSet()) {
            _active.erase (_active.begin() + _marginBegin);
            _active.insert(_active.begin() + _marginEnd - 1, Line(getCols()));

            damageRange(_marginBegin, _marginEnd);
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

            _active.push_back(Line(getCols()));

            damageAll();
            _barDamage = true;
        }

        clearSelection();
    }

    void insertLines(uint16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
               ", margin-end=" << _marginEnd);

        _active.erase (_active.begin() + _marginEnd - n,
                       _active.begin() + _marginEnd);
        _active.insert(_active.begin() + row, n, Line(getCols()));

        damageRange(row, _marginEnd);

        clearSelection();
    }

    void eraseLines(uint16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
               ", margin-end=" << _marginEnd);

        _active.erase (_active.begin() + row,
                       _active.begin() + row + n);
        _active.insert(_active.begin() + _marginEnd - n, n, Line(getCols()));

        damageRange(row, _marginEnd);

        clearSelection();
    }

    void scrollUpMargins(uint16_t n) {
        ASSERT(n <= _marginEnd - _marginBegin, "");
        eraseLines(_marginBegin, n);
    }

    void scrollDownMargins(uint16_t n) {
        ASSERT(n <= _marginEnd - _marginBegin, "");
        insertLines(_marginBegin, n);
    }

    void clearLine(uint16_t row) {
        ASSERT(row < getRows(), "");
        _active[row].clear();
    }

    void clearLineLeft(Pos pos) {
        _active[pos.row].clearLeft(pos.col);
    }

    void clearLineRight(Pos pos) {
        _active[pos.row].clearRight(pos.col);
    }

    void clear() {
        for (auto & l : _active) { l.clear(); }
        clearSelection();
    }

    void clearAbove(uint16_t row) {
        for (auto i = _active.begin(); i != _active.begin() + row; ++i) {
            i->clear();
        }
        clearSelection();
    }

    void clearBelow(uint16_t row) {
        for (auto i = _active.begin() + row; i != _active.end(); ++i) {
            i->clear();
        }
        clearSelection();
    }

    // Called by Terminal::damageCursor()
    void damageCell(Pos pos) {
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col < getCols(), "");
        _active[pos.row].damageAdd(pos.col, pos.col + 1);
    }

    // Called by Terminal::fixDamage()
    void resetDamage() {
        for (auto & l : _active) { l.resetDamage(); }
        _barDamage = false;     // XXX ?
    }

    void damageAll() {
        for (auto & l : _active) { l.damageAll(); }
        _barDamage = true;      // XXX ?
    }

    void damageRange(uint16_t begin_, uint16_t end_) {
        ASSERT(begin_ <= end_, "");
        ASSERT(end_ <= _marginEnd, "");
        for (auto i = _active.begin() + begin_; i != _active.begin() + end_; ++i) {
            i->damageAll();
        }
    }

    void dump(std::ostream & ost) const {
        for (const auto & l : _active) {
            ost << l.isContinuation() << " " << std::setw(3) << l.getWrap() << " \'";

            uint16_t col = 0;

            ost << SGR::UNDERLINE;
            for (; col != l.getWrap(); ++col) {
                ost << l.getCell(col).seq;
            }
            ost << SGR::RESET_UNDERLINE;

            for (; col != getCols(); ++col) {
                ost << l.getCell(col).seq;
            }

            ost << "\'" << std::endl;
        }
        ost << std::endl;
    }
};

#endif // COMMON__BUFFER__HXX
