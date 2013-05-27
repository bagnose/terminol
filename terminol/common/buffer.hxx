// vi:noai:sw=4

#ifndef COMMON__BUFFER__HXX
#define COMMON__BUFFER__HXX

#include "terminol/common/cell.hxx"
#include "terminol/common/config.hxx"

#include <unordered_map>
#include <deque>
#include <vector>

#include <cstddef>
#include <stdint.h>

// TODO
// - Separate history from current
// - Introduce dedupe
// - Explicit buffer width
// - CurrentLine keeps trailing non-blank cells on resize

namespace {

    template <class T>
    T sdbm(const void * buf, size_t bufLength) {
        const uint8_t * buffer = static_cast<const uint8_t *>(buf);

        T hashAddress = 0;
        for (size_t i = 0; i != bufLength; ++i) {
            hashAddress = buffer[i] + (hashAddress << 6) + (hashAddress << 16) - hashAddress;
        }
        return hashAddress;
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

private:
    struct LineInfo {
        explicit LineInfo(std::vector<Cell> && cells_) :
            cells(cells_),
            refCount(1) {}

        std::vector<Cell> cells;
        uint32_t          refCount;
    };

    std::unordered_map<Tag, LineInfo> _lines;
    size_t                            _totalRefCount;

public:
    Deduper() : _lines(), _totalRefCount(0) {}

    Tag store(std::vector<Cell> && cells) {
        auto tag = makeTag(cells);

again:
        auto iter = _lines.find(tag);

        if (iter == _lines.end()) {
            _lines.insert(std::make_pair(tag, LineInfo(std::move(cells))));
        }
        else {
            auto & lineInfo = iter->second;

            if (cells != lineInfo.cells) {
#if DEBUG
                PRINT("Collision between:");
                for (auto c : cells) { std::cerr << c.seq; }
                std::cerr << std::endl;
                PRINT("And:");
                for (auto c : lineInfo.cells) { std::cerr << c.seq; }
                std::cerr << std::endl;
#endif // DEBUG

                ENFORCE(static_cast<Tag>(_lines.size()) != 0, "No dedupe room left");

                ++tag;
                goto again;
            }

            ++lineInfo.refCount;
        }

        ++_totalRefCount;

#if DEBUG
        if (_totalRefCount != 0) {
            PRINT("+++ " << 100.0 * double(_lines.size()) / double(_totalRefCount) << " %");
        }
#endif

        return tag;
    }

    const std::vector<Cell> & lookup(Tag tag) const {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        const auto & lineInfo = iter->second;
        return lineInfo.cells;
    }

    void remove(Tag tag) {
        auto iter = _lines.find(tag);
        ASSERT(iter != _lines.end(), "");
        auto & lineInfo = iter->second;

        if (--lineInfo.refCount == 0) {
            _lines.erase(iter);
        }

        --_totalRefCount;

#if DEBUG
        if (_totalRefCount != 0) {
            PRINT("--- " << 100.0 * double(_lines.size()) / double(_totalRefCount) << " %");
        }
#endif
    }

private:
    Tag makeTag(const std::vector<Cell> & cells) const {
        return sdbm<Tag>(&cells.front(), sizeof(Cell) * cells.size());
    }
};

//
//
//

class Line {
    uint16_t          _cols;        // May be more cells than this due to resize.
    std::vector<Cell> _cells;
    uint16_t          _damageBegin;
    uint16_t          _damageEnd;

    // XXX are these useful?
    std::vector<Cell>::iterator begin() { return _cells.begin(); }
    std::vector<Cell>::iterator end()   { return _cells.end(); }

public:
    std::vector<Cell>::const_iterator begin() const { return _cells.begin(); }
    std::vector<Cell>::const_iterator end()   const { return _cells.end(); }

    explicit Line(uint16_t cols) :
        _cols(cols),
        _cells(cols, Cell::blank())
    {
        damageAll();
    }

    Line(uint16_t cols, const std::vector<Cell> & cells) :
        _cols(cols),
        _cells(cells)
    {
        resize(cols);
        damageAll();
    }

    const std::vector<Cell> & cells() const { return _cells; }

    const Cell & nth(uint16_t col) const {
        ASSERT(col < _cols, "");
        return _cells[col];
    }

    void getDamage(uint16_t & colBegin, uint16_t & colEnd) const {
        colBegin = _damageBegin;
        colEnd   = _damageEnd;
    }

    void insert(uint16_t col, uint16_t n) {
        trim();

        std::copy_backward(&_cells[col],
                           &_cells[_cells.size() - n],
                           &_cells[_cells.size()]);
        std::fill(&_cells[col], &_cells[col + n], Cell::blank());

        damageAdd(col, _cells.size());
    }

    void erase(uint16_t col, uint16_t n) {
        trim();

        PRINT("Erase: col=" << col << " n=" << n << " _cols=" << _cols << " size=" << _cells.size());

        ASSERT(col + n <= _cells.size(), "");

        std::copy(&_cells[col] + n, &_cells[_cells.size()], &_cells[col]);
        std::fill(&_cells[_cells.size()] - n, &_cells[_cells.size()], Cell::blank());

        damageAdd(col, _cells.size());
    }

    void setCell(uint16_t col, const Cell & cell) {
        trim();

        ASSERT(col < _cells.size(), "");
        _cells[col] = cell;

        damageAdd(col, col + 1);
    }

    void resize(uint16_t cols) {
        if (_cells.size() < cols) {
            uint16_t oldCols = _cells.size();
            _cells.resize(cols, Cell::blank());
            damageAdd(oldCols, cols);
        }

        _cols = cols;
    }

    void clear() {
        trim();

        std::fill(begin(), end(), Cell::blank());
        damageAll();
    }

    void clearLeft(uint16_t endCol) {
        trim();

        std::fill(begin(), begin() + endCol, Cell::blank());
        damageAdd(0, endCol);
    }

    void clearRight(uint16_t beginCol) {
        trim();

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
            _cells.erase(_cells.begin() + _cols, _cells.end());
        }
    }
};

//
//
//

class Buffer {
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
        _barDamage(true)
    {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");

        _config.getScrollbarWidth();        // XXX avoid unused error
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

    uint16_t getMarginBegin() const { return _marginBegin; }
    uint16_t getMarginEnd()   const { return _marginEnd;   }

    bool     getBarDamage()   const { return _barDamage; }

    bool scrollUp(uint16_t rows) {
        auto oldScrollOffset = _scrollOffset;

        if (_scrollOffset + rows > getHistory()) {
            _scrollOffset = getHistory();
        }
        else {
            _scrollOffset += rows;
        }

        return _scrollOffset != oldScrollOffset;
    }

    bool scrollDown(uint16_t rows) {
        auto oldScrollOffset = _scrollOffset;

        if (rows > _scrollOffset) {
            _scrollOffset = 0;
        }
        else {
            _scrollOffset -= rows;
        }

        return _scrollOffset != oldScrollOffset;
    }

    bool scrollTop() {
        if (_scrollOffset != getHistory()) {
            _scrollOffset = getHistory();
            return true;
        }
        else {
            return false;
        }
    }

    bool scrollBottom() {
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

        auto cells = _active.front().cells();
        _active.pop_front();
        while (!cells.empty() && cells.back() == Cell::blank()) {
            cells.pop_back();
        }
        cells.shrink_to_fit();
        auto tag = _deduper.store(std::move(cells));

        if (_history.size() == _historyLimit) {
            _deduper.remove(_history.front());
            _history.pop_front();
        }

        _history.push_back(tag);

        ASSERT(!_active.empty(), "");
    }

    void unbump() {
        ASSERT(!_history.empty(), "");

        auto tag = _history.back();
        const auto & cells = _deduper.lookup(tag);
        _active.push_front(Line(_cols, cells));
        _history.pop_back();
        _deduper.remove(tag);
    }

public:

    //
    // Outgoing / scroll-relative operations.
    //

    const Cell & getCell(uint16_t row, uint16_t col) const {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");

        if (row < _scrollOffset) {
            auto tag = _history[_history.size() - _scrollOffset + row];
            const auto & cells = _deduper.lookup(tag);
            return col < cells.size() ? cells[col] : BLANK;
        }
        else {
            return _active[row - _scrollOffset].nth(col);
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

    //
    // Incoming / active-relative operations.
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

    void setCell(Pos pos, const Cell & cell) {
        ASSERT(cell.seq.lead() != NUL, "");
        ASSERT(pos.row < getRows(), "");
        ASSERT(pos.col < getCols(), "");
        _active[pos.row].setCell(pos.col, cell);
    }

    int16_t resize(uint16_t rows, uint16_t cols) {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");

        int16_t delta = 0;

        //size_t oldHistory = _history;

        //
        // Cols
        //

        if (cols != getCols()) {
            for (auto & line : _active) {
                line.resize(cols);
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
                ++delta;
            }

            // And resize the container to obtain the rest.

            _active.resize(rows, Line(_cols));
        }
        else if (rows < getRows()) {
            // Remove blanks lines from the back.
            while (getRows() != rows) {
                if (_active.back().isBlank()) {
                    _active.pop_back();
                }
                else {
                    break;
                }
            }

            delta = rows - getRows();

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
            if (_config.getScrollOnResize()) {
                // Scroll to bottom
                _scrollOffset = 0;
            }
            else {
                // Preserve the top line
                _scrollOffset = _scrollOffset - delta;
                ASSERT(!(_scrollOffset > _history.size()), "");
            }
        }

        _marginBegin = 0;
        _marginEnd   = rows;
        _barDamage   = true;

        _active.shrink_to_fit();

        damageAll();

        return delta;
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

                if (!_config.getScrollWithHistory()) {
                    if (_scrollOffset != 0 && _scrollOffset != _history.size()) {
                        ++_scrollOffset;
                    }
                }
            }

            _active.push_back(Line(getCols()));

            damageAll();
            _barDamage = true;
        }
    }

    void insertLines(uint16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n
               << ", margin-end=" << _marginEnd);

        _active.erase (_active.begin() + _marginEnd - n,
                       _active.begin() + _marginEnd);
        _active.insert(_active.begin() + row, n, Line(getCols()));

        damageRange(row, _marginEnd);
    }

    void eraseLines(uint16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n
               << ", margin-end=" << _marginEnd);

        _active.erase (_active.begin() + row,
                       _active.begin() + row + n);
        _active.insert(_active.begin() + _marginEnd - n, n, Line(getCols()));

        damageRange(row, _marginEnd);
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
    }

    void clearAbove(uint16_t row) {
        for (auto i = _active.begin(); i != _active.begin() + row; ++i) {
            i->clear();
        }
    }

    void clearBelow(uint16_t row) {
        for (auto i = _active.begin() + row; i != _active.end(); ++i) {
            i->clear();
        }
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
            for (const auto & c : l) {
                ost << c.seq;
            }
            ost << std::endl;
        }
    }
};

#endif // COMMON__BUFFER__HXX
