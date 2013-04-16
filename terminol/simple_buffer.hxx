// vi:noai:sw=4

#ifndef SIMPLE_BUFFER__HXX
#define SIMPLE_BUFFER__HXX

#include "terminol/common.hxx"
#include "terminol/char.hxx"

#include <vector>
#include <deque>

class SimpleBuffer {
    class Line {
        std::vector<Char> _chars;

    public:
        explicit Line(uint16_t cols) : _chars(cols, Char::blank()) {}

        uint16_t getCols() const { return static_cast<uint16_t>(_chars.size()); }
        const Char & getChar(uint16_t col) const { return _chars[col]; }

        void insert(uint16_t beforeCol, uint16_t n) {
            std::copy_backward(
                    &_chars[beforeCol],
                    &_chars[_chars.size() - n],
                    &_chars[_chars.size()]);
            std::fill(&_chars[beforeCol], &_chars[beforeCol + n], Char::blank());
        }

        void erase(uint16_t col, uint16_t n) {
            ASSERT(col < getCols(), "");
            std::copy(&_chars[col], &_chars[_chars.size()], &_chars[col] - n);
            std::fill(&_chars[_chars.size()] - n, &_chars[_chars.size()], Char::blank());
        }

        void set(uint16_t col, const Char & ch) {
            ASSERT(col < getCols(), "");
            _chars[col] = ch;
        }

        void clear() {
            std::fill(_chars.begin(), _chars.end(), Char::blank());
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

    const Char & getChar(uint16_t row, uint16_t col) const {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        return _lines[row].getChar(col);
    }

    void insertChars(uint16_t row, uint16_t beforeCol, uint16_t n) {
        ASSERT(row < getRows(), "");
        ASSERT(beforeCol <= getCols(), "");
        _lines[row].insert(beforeCol, n);
    }

    void eraseChars(uint16_t row, uint16_t col, uint16_t n) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[row].erase(col, n);
    }

    void set(uint16_t row, uint16_t col, const Char & ch) {
        ASSERT(row < getRows(), "");
        ASSERT(col < getCols(), "");
        _lines[row].set(col, ch);
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

#endif // SIMPLE_BUFFER__HXX
