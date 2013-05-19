// vi:noai:sw=4

#ifndef COMMON__BUFFER__HXX
#define COMMON__BUFFER__HXX

#include "terminol/common/cell.hxx"
#include "terminol/common/config.hxx"
#include "terminol/support/debug.hxx"

#include <vector>
#include <deque>

class Buffer {
    class Line {
        std::vector<Cell> _cells;
        uint16_t          _damageBegin;
        uint16_t          _damageEnd;

    public:
        explicit Line(uint16_t cols) :
            _cells(cols, Cell::blank())
        {
            damageAll();
        }

        uint16_t getCols() const { return static_cast<uint16_t>(_cells.size()); }
        const Cell & getCell(uint16_t col) const { return _cells[col]; }

        void getDamage(uint16_t & colBegin, uint16_t & colEnd) const {
            colBegin = _damageBegin;
            colEnd   = _damageEnd;
        }

        void insert(uint16_t beforeCol, uint16_t n) {
            std::copy_backward(&_cells[beforeCol],
                               &_cells[_cells.size() - n],
                               &_cells[_cells.size()]);
            std::fill(&_cells[beforeCol], &_cells[beforeCol + n], Cell::blank());

            damageAdd(beforeCol, getCols());
        }

        void erase(uint16_t col, uint16_t n) {
            ASSERT(col + n <= getCols(), "");

            std::copy(&_cells[col] + n, &_cells[_cells.size()], &_cells[col]);
            std::fill(&_cells[_cells.size()] - n, &_cells[_cells.size()], Cell::blank());

            damageAdd(col, getCols());
        }

        void setCell(uint16_t col, const Cell & cell) {
            ASSERT(col < getCols(), "");
            _cells[col] = cell;

            damageAdd(col, col + 1);
        }

        void resize(uint16_t cols) {
            uint16_t oldCols = getCols();
            _cells.resize(cols, Cell::blank());

            if (cols > oldCols) {
                damageAdd(oldCols, cols);
            }
            else {
                _damageBegin = std::min(_damageBegin, cols);
                _damageEnd   = std::min(_damageEnd,   cols);
            }
        }

        void clear() {
            std::fill(_cells.begin(), _cells.end(), Cell::blank());
            damageAll();
        }

        void clearLeft(uint16_t endCol) {
            std::fill(_cells.begin(), _cells.begin() + endCol, Cell::blank());
            damageAdd(0, endCol);
        }

        void clearRight(uint16_t beginCol) {
            std::fill(_cells.begin() + beginCol, _cells.end(), Cell::blank());
            damageAdd(beginCol, getCols());
        }

        bool isBlank() const {
            for (const auto & c : _cells) {
                if (c != Cell::blank()) { return false; }
            }
            return true;
        }

        void resetDamage() {
            _damageBegin = _damageEnd = 0;
        }

        void damageAll() {
            _damageBegin = 0;
            _damageEnd   = getCols();
        }

        void damageAdd(uint16_t begin, uint16_t end) {
            ASSERT(begin <  end, "");
            ASSERT(end   <= getCols(), "");

            if (_damageBegin == _damageEnd) {
                // No damage yet.
                _damageBegin = begin;
                _damageEnd   = end;
            }
            else {
                _damageBegin = std::min(_damageBegin, begin);
                _damageEnd   = std::max(_damageEnd,   end);
            }
        }
    };

    //
    //
    //

    const Config     & _config;

    std::deque<Line>   _lines;

    size_t             _maxHistory;
    size_t             _history;          // Offset of active region.
    size_t             _scroll;           // Offset of visible region.

    bool               _barDamage;        // Scrollbar damage.

    uint16_t           _marginBegin;
    uint16_t           _marginEnd;


public:
    Buffer(const Config & config, uint16_t rows, uint16_t cols, size_t maxHistory) :
        _config(config),
        _lines(rows, Line(cols)),
        _maxHistory(maxHistory),
        _history(0),
        _scroll(0),
        _barDamage(true),
        //
        _marginBegin(0),
        _marginEnd(rows)
    {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");
    }

    size_t   getHistory()     const { return _history; }
    size_t   getScroll()      const { return _scroll;  }
    size_t   getTotalRows()   const { return _lines.size(); }

    uint16_t getRows()        const { return _lines.size() - _history; }
    uint16_t getCols()        const { return _lines.front().getCols(); }

    uint16_t getMarginBegin() const { return _marginBegin; }
    uint16_t getMarginEnd()   const { return _marginEnd;   }

    bool     getBarDamage()   const { return _barDamage; }

    bool scrollUp(uint16_t rows) {
        size_t oldScroll = _scroll;

        if (rows > _scroll) {
            _scroll = 0;
        }
        else {
            _scroll -= rows;
        }

        return _scroll != oldScroll;
    }

    bool scrollDown(uint16_t rows) {
        size_t oldScroll = _scroll;

        _scroll = std::min(_history, _scroll + rows);

        return _scroll != oldScroll;
    }

    bool scrollTop() {
        if (_scroll != 0) {
            _scroll = 0;
            return true;
        }
        else {
            return false;
        }
    }

    bool scrollBottom() {
        if (_scroll != _history) {
            _scroll = _history;
            return true;
        }
        else {
            return false;
        }
    }

    void setMargins(uint16_t begin, uint16_t end) {
        ASSERT(begin < end, "");
        ASSERT(end <= getRows(), "");
        _marginBegin = begin;
        _marginEnd   = end;
    }

    void resetMargins() {
        _marginBegin = 0;
        _marginEnd   = getRows();
    }

    // XXX Do we need an explicit bool covering this or is it sufficient
    // to infer it from the values?
    bool marginsSet() const {
        return _marginBegin != 0 || _marginEnd != getRows();
    }

    //
    // Outgoing / scroll-relative operations.
    //

    const Cell & getCell(uint16_t row, uint16_t col) const {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        return _lines[_scroll + row].getCell(col);
    }

    void getDamage(uint16_t row, uint16_t & colBegin, uint16_t & colEnd) const {
        ASSERT(row < getRows(), "");
        _lines[_scroll + row].getDamage(colBegin, colEnd);
    }

    //
    // Incoming / history-relative operations.
    //

    void insertCells(uint16_t row, uint16_t beforeCol, uint16_t n) {
        ASSERT(row < getRows(), "");
        ASSERT(beforeCol <= getCols(), "");
        _lines[_history + row].insert(beforeCol, n);
    }

    void eraseCells(uint16_t row, uint16_t col, uint16_t n) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[_history + row].erase(col, n);
    }

    void setCell(uint16_t row, uint16_t col, const Cell & cell) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[_history + row].setCell(col, cell);
    }

    int16_t resize(uint16_t rows, uint16_t cols) {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");

        size_t oldHistory = _history;

        if (cols != getCols()) {
            for (auto & line : _lines) {
                line.resize(cols);
            }
        }

        if (rows > getRows()) {
            // The scrollback history is the first port of call for rows.
            if (_lines.size() >= rows) {
                // History can provide it all.
                _history = _lines.size() - rows;
            }
            else {
                // Get what we can (if any) from history.
                _history = 0;

                // And resize the container to obtain the rest.
                _lines.resize(rows, Line(cols));
            }
        }
        else if (rows < getRows()) {
            // Remove blanks lines from the back.
            while (getRows() != rows) {
                if (_lines.back().isBlank()) {
                    _lines.pop_back();
                }
                else {
                    break;
                }
            }

            // And push the rest into history.
            _history = _lines.size() - rows;

            // Then enforce the history limit.
            if (_history > _maxHistory) {
                _lines.erase(_lines.begin(), _lines.begin() + _history - _maxHistory);
                _history = _maxHistory;
            }
        }

        damageAll();
        _scroll = _history;         // XXX Scroll to bottom on resize? Most terminals do

        _marginBegin = 0;
        _marginEnd   = rows;
        _barDamage   = true;

        return oldHistory - _history;       // FIXME I'm not sure this is quite right.
    }

    void addLine() {
        if (marginsSet()) {
            _lines.insert(_lines.begin() + _history + _marginEnd, Line(getCols()));
            _lines.erase(_lines.begin() + _history + _marginBegin);

            damageRange(_marginBegin, _marginEnd);
        }
        else {
            _lines.push_back(Line(getCols()));
            if (_history == _maxHistory) {
                _lines.pop_front();

                if (_scroll != _history) {
                    if (!_config.getScrollWithHistory() && _scroll != 0) {
                        --_scroll;
                    }
                }
            }
            else {
                if (_scroll != _history) {
                    if (_config.getScrollWithHistory() && _scroll != 0)
                    {
                        --_scroll;
                    }
                }
                else {
                    ++_scroll;
                }

                ++_history;
            }

            damageAll();
            _barDamage = true;
        }
    }

    void insertLines(uint16_t beforeRow, uint16_t n) {
        ASSERT(beforeRow >= _marginBegin && beforeRow < _marginEnd, "");
        ASSERT(beforeRow + n <= _marginEnd, "");

#if 0
        _lines.erase(_lines.begin() + _history + _marginEnd - n,
                     _lines.begin() + _history + _marginEnd);
        _lines.insert(_lines.begin() + _history + beforeRow, n, Line(getCols()));
#else
        // This path can handle any value of n
        _lines.insert(_lines.begin() + _history + beforeRow, n, Line(getCols()));
        _lines.erase(_lines.begin() + _history + _marginEnd,
                     _lines.begin() + _history + _marginEnd + n);
#endif

        damageRange(beforeRow, _marginEnd);
    }

    void eraseLines(uint16_t row, uint16_t n) {
        ASSERT(row >= _marginBegin && row < _marginEnd, "");
        ASSERT(row + n <= _marginEnd, "");

        _lines.insert(_lines.begin() + _history + _marginEnd, n, Line(getCols()));
        _lines.erase(_lines.begin() + _history + row, _lines.begin() + _history + row + n);

        damageRange(row, _marginEnd);
    }

    void clearLine(uint16_t row) {
        ASSERT(row < getRows(), "");
        _lines[_history + row].clear();
    }

    void clearLineLeft(uint16_t row, uint16_t endCol) {
        _lines[_history + row].clearLeft(endCol);
    }

    void clearLineRight(uint16_t row, uint16_t beginCol) {
        _lines[_history + row].clearRight(beginCol);
    }

    void clear() {
        for (auto i = _lines.begin() + _history; i != _lines.end(); ++i) {
            i->clear();
        }
    }

    void clearAbove(uint16_t endRow) {
        for (auto i = _lines.begin() + _history;
             i != _lines.begin() + _history + endRow; ++i)
        {
            i->clear();
        }
    }

    void clearBelow(uint16_t beginRow) {
        for (auto i = _lines.begin() + _history + beginRow; i != _lines.end(); ++i)
        {
            i->clear();
        }
    }

    // Called by Terminal::damageCursor()
    void damageCell(uint16_t row, uint16_t col) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[_history + row].damageAdd(col, col + 1);
    }

    // Called by Terminal::fixDamage()
    void resetDamage() {
        for (auto i = _lines.begin() + _history; i != _lines.end(); ++i) {
            i->resetDamage();
        }

        _barDamage = false;     // XXX ?
    }

    void damageAll() {
        for (auto i = _lines.begin() + _history; i != _lines.end(); ++i) {
            i->damageAll();
        }

        _barDamage = true;      // XXX ?
    }

    void damageRange(uint16_t begin, uint16_t end) {
        for (auto i = _lines.begin() + _history + begin;
             i != _lines.begin() + _history + end; ++i)
        {
            i->damageAll();
        }
    }
};

#endif // COMMON__BUFFER__HXX
