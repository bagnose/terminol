// vi:noai:sw=4

#ifndef COMMON__SIMPLE_BUFFER__HXX
#define COMMON__SIMPLE_BUFFER__HXX

#include "terminol/common/support.hxx"
#include "terminol/common/cell.hxx"

#include <vector>
#include <deque>

class SimpleBuffer {
    class Line {
        std::vector<Cell> _cells;

    public:
        explicit Line(uint16_t cols) : _cells(cols, Cell::blank()) {}

        uint16_t getCols() const { return static_cast<uint16_t>(_cells.size()); }
        const Cell & getCell(uint16_t col) const { return _cells[col]; }

        void insert(uint16_t beforeCol, uint16_t n) {
            std::copy_backward(
                    &_cells[beforeCol],
                    &_cells[_cells.size() - n],
                    &_cells[_cells.size()]);
            std::fill(&_cells[beforeCol], &_cells[beforeCol + n], Cell::blank());
        }

        void erase(uint16_t col, uint16_t n) {
            ASSERT(col + n < getCols(), "");
            std::copy(&_cells[col], &_cells[_cells.size()], &_cells[col] - n);
            std::fill(&_cells[_cells.size()] - n, &_cells[_cells.size()], Cell::blank());
        }

        void set(uint16_t col, const Cell & cell) {
            ASSERT(col < getCols(), "");
            _cells[col] = cell;
        }

        void clear() {
            std::fill(_cells.begin(), _cells.end(), Cell::blank());
        }
    };

    //
    //
    //

    std::deque<Line> _lines;

public:
    SimpleBuffer(uint16_t rows, uint16_t cols) :
        _lines(rows, Line(cols))
    {
        ASSERT(rows != 0, "");
        ASSERT(cols != 0, "");
    }

    uint16_t getRows() const { return _lines.size(); }
    uint16_t getCols() const { return _lines.front().getCols(); }

    const Cell & getCell(uint16_t row, uint16_t col) const {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        return _lines[row].getCell(col);
    }

    void insertCells(uint16_t row, uint16_t beforeCol, uint16_t n) {
        ASSERT(row < getRows(), "");
        ASSERT(beforeCol <= getCols(), "");
        _lines[row].insert(beforeCol, n);
    }

    void eraseCells(uint16_t row, uint16_t col, uint16_t n) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[row].erase(col, n);
    }

    void set(uint16_t row, uint16_t col, const Cell & cell) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[row].set(col, cell);
    }

    void resize(uint16_t rows, uint16_t cols) {
        _lines.clear();
        _lines.resize(rows, Line(cols));
    }

    void addLine() {
        _lines.push_back(Line(getCols()));
        _lines.pop_front();
    }

    void insertLines(uint16_t beforeRow, uint16_t n) {
        ASSERT(beforeRow < getRows() + 1, "");
        for (uint16_t i = 0; i != n; ++i) {
            _lines.insert(_lines.begin() + beforeRow, Line(getCols()));
        }
        for (uint16_t i = 0; i != n; ++i) {
            _lines.pop_front();     // Or back?
        }
    }

    void eraseLines(uint16_t row, uint16_t n) {
        ASSERT(row + n < getRows(), "");
        _lines.erase(_lines.begin() + row, _lines.begin() + row + n);
        for (uint16_t i = 0; i != n; ++i) {
            _lines.push_back(Line(getCols()));
        }
    }

    void clearLine(uint16_t row) {
        ASSERT(row < getRows(), "");
        _lines[row].clear();
    }

    void clearAll() {
        for (auto & line : _lines) {
            line.clear();
        }
    }
};

void dump(std::ostream & ost, const SimpleBuffer & buffer);

#endif // COMMON__SIMPLE_BUFFER__HXX
