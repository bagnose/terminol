// vi:noai:sw=4

#ifndef BUFFER__HXX
#define BUFFER__HXX

#include "terminol/common/cell.hxx"
#include "terminol/support/support.hxx"

#include <vector>
#include <deque>
#include <iostream>
#include <cstdlib>

class RawBuffer : protected Uncopyable {
    struct Line {
        std::vector<Cell> chars;

        size_t size() const { return chars.size(); }

        void insert(const Cell & cell, size_t col) {
            ASSERT(!(col > size()), "");
            chars.insert(chars.begin() + col, cell);
        }
    };

    std::deque<Line> _lines;
    size_t           _sizeLimit;

public:
    RawBuffer(size_t size, size_t sizeLimit) :
        _lines(size, Line()),
        _sizeLimit(sizeLimit)
    {
        ASSERT(!(sizeLimit < size), "");
    }

    size_t getSize() const { return _lines.size(); }

    uint16_t getWidth(size_t row) const {
        ASSERT(row < _lines.size(), "");
        const Line & line = _lines[row];
        return line.size();
    }

    const Cell & getCell(size_t row, uint16_t col) const {
        ASSERT(row < _lines.size(), "");
        const Line & line = _lines[row];
        return line.chars[col];
    }

    bool addLine() {
        bool full = _lines.size() == _sizeLimit;
        if (full) _lines.pop_front();
        _lines.push_back(Line());
        return !full;
    }

    void insertCell(const Cell & cell, size_t row, uint16_t col) {
        ASSERT(row < _lines.size(), "");
        Line & line = _lines[row];
        ASSERT(!(col > line.chars.size()), "");
        line.chars.insert(line.chars.begin() + col, cell);
    }

    void eraseCell(size_t row, uint16_t col) {
        ASSERT(row < _lines.size(), "");
        Line & line = _lines[row];
        ASSERT(col < line.chars.size(), "");
        line.chars.erase(line.chars.begin() + col);
    }

    void clear() {
        _lines.clear();
    }
};

inline void dumpRawBuffer(const RawBuffer & buffer) {
    size_t rows = buffer.getSize();
    std::cout << "Lines: " << rows << std::endl;
    for (size_t r = 0; r != rows; ++r) {
        size_t cols = buffer.getWidth(r);
        std::cout << r << " ";
        for (size_t c = 0; c != cols; ++c) {
            std::cout << buffer.getCell(r, c);
        }
        std::cout << std::endl;
    }
}

//
//
//

class WrappedBuffer {
    struct Line {
        size_t   row;
        uint16_t colBegin;
        uint16_t colEnd;

        Line(size_t row_, uint16_t colBegin_, uint16_t colEnd_) :
            row(row_), colBegin(colBegin_), colEnd(colEnd_) {}

        uint16_t getWidth() const { return colEnd - colBegin; }
    };

    RawBuffer        _raw;
    std::deque<Line> _lines;
    size_t           _offset;
    uint16_t         _wrapCol;

public:
    WrappedBuffer(size_t wrapCol, size_t size, size_t sizeLimit) :
        _raw(size, sizeLimit),
        _lines(),
        _offset(0),
        _wrapCol(wrapCol)
    {
        ASSERT(!(sizeLimit < size), "");

        for (size_t i = 0; i != size; ++i) {
            _lines.push_back(Line(i, 0, 0));
        }
    }

    size_t getSize() const { return _lines.size(); }

    uint16_t getWrapCol() const { return _wrapCol; }

    uint16_t getWidth(size_t row) const {
        ASSERT(row < _lines.size(), "");
        const Line & line = _lines[row];
        return line.getWidth();
    }

    const Cell & getCell(size_t row, uint16_t col) const {
        const Line & line = _lines[row];
        return _raw.getCell(line.row - _offset, line.colBegin + col);
    }

    void setWrapCol(uint16_t wrapCol) {
        if (_wrapCol == wrapCol) { return; }    // optimisation

        _lines.clear();
        _wrapCol = wrapCol;

        for (size_t row = 0; row != _raw.getSize(); ++row) {
            size_t width = _raw.getWidth(row);
            size_t col = 0;
            do {
                size_t colBegin = col;
                col += _wrapCol;
                size_t colEnd = std::min(col, width);
                _lines.push_back(Line(row, colBegin, colEnd));
            } while (col < width);
        }
    }

    bool addLine() {
        bool grew = _raw.addLine();

        if (!grew) {
            ASSERT(!_lines.empty(), "");
            size_t row = _lines.front().row;
            do {
                _lines.pop_front();
            } while (!_lines.empty() && _lines.front().row == row);
            ++_offset;
        }

        _lines.push_back(Line(_raw.getSize() - 1, 0, 0));

        return grew;    // XXX who uses this return value?
    }

    void insertCell(const Cell & cell, size_t row, uint16_t col) {
        ASSERT(row < _lines.size(), "");
        Line & line = _lines[row];
        ASSERT(!(col > line.colEnd), "");
        _raw.insertCell(cell, line.row - _offset, line.colBegin + col);
        ++line.colEnd;
        // TODO deal with wrapping
    }

    void eraseCell(size_t row, uint16_t col) {
        ASSERT(row < _lines.size(), "");
        Line & line = _lines[row];
        ASSERT(col < line.colEnd, "");
        _raw.eraseCell(line.row - _offset, line.colBegin + col);
        --line.colEnd;
        // TODO deal with unwrapping
    }

    void clear() {
        _lines.clear();
        _raw.clear();
        addLine();      // XXX
    }
};

inline void dumpWrappedBuffer(const WrappedBuffer & buffer) {
    size_t rows = buffer.getSize();
    std::cout << "Lines: " << rows << std::endl;
    for (size_t r = 0; r != rows; ++r) {
        size_t cols = buffer.getWidth(r);
        std::cout << r << " ";
        for (size_t c = 0; c != cols; ++c) {
            std::cout << buffer.getCell(r, c);
        }
        std::cout << std::endl;
    }
}

#endif // BUFFER__HXX
