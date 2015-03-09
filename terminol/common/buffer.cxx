// vi:noai:sw=4
// Copyright © 2013-2015 David Bryant

#include "terminol/common/buffer.hxx"
#include "terminol/common/escape.hxx"

Buffer::ParaIter::ParaIter(const Buffer & buffer, APos pos) :
    _buffer(buffer),
    _pos(pos),
    _cells(_buffer.getCols(), Cell::blank())
{
    _buffer.getLine(_pos.row, _cells, _cont, _wrap);
    _valid = _pos.col < _wrap;
}

void Buffer::ParaIter::moveForward() {
    ASSERT(_valid, "Invalid");

    ++_pos.col;

    if (_pos.col == _buffer.getCols()) {
        ++_pos.row;
        _pos.col = 0;

        if (_cont) {
            ASSERT(_pos.row < _buffer.getRows(), "");
            _buffer.getLine(_pos.row, _cells, _cont, _wrap);
            _valid = _pos.col < _wrap;
        }
        else {
            _valid = false;
        }
    }
    else {
        _valid = _pos.col < _wrap;
    }
}

void Buffer::ParaIter::moveBackward() {
    ASSERT(_valid, "Invalid");

    if (_pos.col == 0) {
        --_pos.row;
        _pos.col = _buffer.getCols() - 1;

        if (_pos.row == -static_cast<int32_t>(_buffer._history.size() + 1)) {
            _valid = false;
        }
        else {
            _buffer.getLine(_pos.row, _cells, _cont, _wrap);
            _valid = _cont;
        }
    }
    else {
        --_pos.col;
    }
}

//
//
//

Buffer::BufferIter::BufferIter(const Buffer & buffer, int32_t row) :
    _buffer(buffer), _row(row), _valid(true)
{
    while (!isStartOfPara()) {
        --_row;

        if (static_cast<uint32_t>(-_row - 1) == _buffer.getHistoricalRows()) {
            _valid = false;
            break;
        }
    }
}

void Buffer::BufferIter::moveForward() {
    do {
        ++_row;

        if (_row == _buffer.getRows()) {
            _valid = false;
            break;
        }
    } while (!isStartOfPara());
}

void Buffer::BufferIter::moveBackward() {
    do {
        --_row;

        if (static_cast<uint32_t>(-_row - 1) == _buffer.getHistoricalRows()) {
            _valid = false;
            break;
        }
    } while (!isStartOfPara());
}

bool Buffer::BufferIter::isStartOfPara() {
    auto prevRow = _row - 1;

    if (static_cast<uint32_t>(-prevRow - 1) == _buffer.getHistoricalRows()) {
        return true;
    }

    if (prevRow < 0) {
        // Historical.
        auto & hline = _buffer._history[_buffer._history.size() + prevRow];
        return hline.seqnum == 0;
    }
    else {
        // Active.
        auto & aline = _buffer._active[prevRow];
        return !aline.cont;
    }
}

Buffer::Buffer(const Config       & config,
               I_Deduper          & deduper,
               AsyncDestroyer     & destroyer,
               int16_t              rows,
               int16_t              cols,
               uint32_t             historyLimit,
               const CharSubArray & charSubs) :
    _config(config),
    _deduper(deduper),
    _destroyer(destroyer),
    _tags(),
    _lostTags(0),
    _pending(),
    _history(),
    _active(rows, ALine(cols)),
    _damage(rows),
    _tabs(cols),
    _scrollOffset(0),
    _historyLimit(historyLimit),
    _cols(cols),
    _barDamage(true),
    _selectMark(),
    _selectDelim(),
    _cursor(),
    _savedCursor(),
    _charSubs(charSubs),
    _search(nullptr)
{
    resetMargins();
    resetTabs();
}

Buffer::~Buffer() {
    if (_search) {
        delete _search;
    }

    class Garbage : public AsyncDestroyer::Garbage {
    private:
        I_Deduper                & _deduper;
        std::deque<I_Deduper::Tag> _tags;
    public:
        Garbage(I_Deduper                   & deduper,
                std::deque<I_Deduper::Tag> && tags) :
            _deduper(deduper), _tags(tags) {}

        ~Garbage() override {
            PRINT("Taking out the garbage!");
            for (auto tag : _tags) {
                // Deregister all of our valid tags. Note, really only the last tags can
                // be invalid.
                if (tag != I_Deduper::invalidTag()) {
                    _deduper.remove(tag);
                }
            }
        }
    };

    _destroyer.add(new Garbage(_deduper, std::move(_tags)));
}

//
//
//

void Buffer::markSelection(Pos pos) {
    damageSelection();
    _selectMark = _selectDelim = APos(pos, _scrollOffset);
}

void Buffer::delimitSelection(Pos pos, bool initial) {
    damageSelection();

    APos apos(pos, _scrollOffset);

    if (initial) {
        // Work out whether hpos is closer to the mark or the delimiter
        // and adjust that one.

        if (_selectDelim < _selectMark) {
            std::swap(_selectMark, _selectDelim);
        }

        auto deltaRow = _selectDelim.row - _selectMark.row;
        auto deltaCol = _selectDelim.col - _selectMark.col;

        auto row = _selectMark.row + deltaRow / 2;
        auto col = _selectMark.col + deltaCol / 2;

        if (deltaRow % 2) { col += getCols() / 2; }

        if (col < 0)          { col += getCols(); --row; }
        if (col >= getCols()) { col -= getCols(); ++row; }

        ASSERT(col >= 0, "Column is not positive.");
        ASSERT(col < getCols(), "Column exceeds maximum.");

        APos centre(row, col);

        if (apos < centre) {
            std::swap(_selectMark, _selectDelim);
        }
    }

    _selectDelim = apos;
    damageSelection();
}

void Buffer::expandSelection(Pos pos, int level) {
    level = level % 4;

    damageSelection();

    if (_selectDelim < _selectMark) {
        std::swap(_selectMark, _selectDelim);
    }

    APos apos(pos, _scrollOffset);

    ParaIter iter(*this, apos);

    if (level == 1) {
        _selectMark = _selectDelim = apos;
    }
    else if (level == 3    ||
             !iter.valid() ||
             iter.getCell().seq.lead() == ' ') {
        ParaIter selectIter(*this, APos(apos.row, 0));

        if (selectIter.valid()) {
            auto delimIter = selectIter;

            do {
                _selectMark = selectIter.getPos();
                selectIter.moveBackward();
            } while (selectIter.valid());

            do {
                _selectDelim = delimIter.getPos();
                delimIter.moveForward();
            } while (delimIter.valid());

            _selectDelim.col = getCols();
        }
        else {
            // Must be an empty line.
        }
    }
    else {
        Regex regex("[" + _config.cutChars + "]");

        auto selectIter = iter;
        auto delimIter  = iter;

        do {
            auto & seq  = selectIter.getCell().seq;
            auto   text = reinterpret_cast<const char *>(&seq.bytes[0]);
            auto   size = static_cast<size_t>(utf8::leadLength(seq.lead()));
            if (!regex.matchTest(text, size)) { break; }
            _selectMark = selectIter.getPos();
            selectIter.moveBackward();
        } while (selectIter.valid());

        do {
            auto & seq  = delimIter.getCell().seq;
            auto   text = reinterpret_cast<const char *>(&seq.bytes[0]);
            auto   size = static_cast<size_t>(utf8::leadLength(seq.lead()));
            if (!regex.matchTest(text, size)) { break; }
            delimIter.moveForward();
            _selectDelim = delimIter.getPos();
        } while (delimIter.valid());
    }

    damageSelection();
}

void Buffer::clearSelection() {
    damageSelection();

    // Retain the selection marker, if it is within the history.
    if (static_cast<int32_t>(_history.size()) + _selectMark.row < 0) {
        _selectMark = APos();
    }

    _selectDelim = _selectMark;
}

bool Buffer::getSelectedText(std::string & text) const {
    APos begin, end;

    // Declare this outside of the loop to avoid reallocation.
    std::vector<Cell> cells(getCols(), Cell::blank());

    if (normaliseSelection(begin, end)) {
        for (auto i = begin; i.row <= end.row; ++i.row, i.col = 0) {
            bool    cont;
            int16_t wrap;
            getLine(i.row, cells, cont, wrap);

            for (; i.col < getCols() && (i.row < end.row || i.col != end.col); ++i.col) {
                if (i.col >= wrap) {
                    text.push_back('\n');
                    break;
                }

                auto & cell = cells[i.col];
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

void Buffer::clearHistory() {
    if (_history.empty()) {
        return;
    }

    for (auto tag : _tags) {
        if (LIKELY(tag != I_Deduper::invalidTag())) {
            _deduper.remove(tag);
        }
    }

    _tags.clear();
    _history.clear();
    _pending.clear();

    clearSelection();

    if (_scrollOffset == 0) {
        _barDamage = true;
    }
    else {
        _scrollOffset = 0;
        damageViewport(true);
    }
}

bool Buffer::scrollUpHistory(uint16_t rows) {
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

bool Buffer::scrollDownHistory(uint16_t rows) {
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

bool Buffer::scrollTopHistory() {
    if (_scrollOffset != getHistoricalRows()) {
        _scrollOffset = getHistoricalRows();
        damageViewport(true);
        return true;
    }
    else {
        return false;
    }
}

bool Buffer::scrollBottomHistory() {
    if (_scrollOffset != 0) {
        _scrollOffset = 0;
        damageViewport(true);
        return true;
    }
    else {
        return false;
    }
}

void Buffer::migrateFrom(Buffer & other, bool clear_) {
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

void Buffer::write(utf8::Seq seq, bool autoWrap, bool insert) {
    damageCell();

    auto cs = getCharSub(_cursor.charSet);
    cs->translate(seq);

    if (autoWrap && _cursor.wrapNext) {
        _cursor.wrapNext = false;
        auto & line = _active[_cursor.pos.row];

        // Don't set 'cont' to true if this line will remain the last line,
        // otherwise we violate our invariant.
        if (_cursor.pos.row == _marginEnd - 1 || _cursor.pos.row < getRows() - 1) {
            line.cont = true; // continues on next line
        }

        ASSERT(_cursor.pos.col == _cols - 1,
               "col=" << _cursor.pos.col << ", _cols-1=" << _cols - 1);
        ASSERT(line.wrap == _cols,
               "wrap=" << line.wrap << ", _cols=" << _cols);
        line.wrap = _cols;

        if (_cursor.pos.row == _marginEnd - 1) {
            addLine();      // invalidates line reference
            moveCursor2(true, 0, false, 0);
        }
        else {
            // If we are on the last line then the column will just be reset.
            moveCursor2(true, 1, false, 0);
        }
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

    if (line.cells[_cursor.pos.col] != Cell::utf8(seq, style)) {
        testClearSelection(APos(_cursor.pos, 0),
                           APos(Pos(_cursor.pos.row, _cursor.pos.col + 1), 0));
        line.cells[_cursor.pos.col] = Cell::utf8(seq, style);
    }

    ASSERT(line.wrap <= getCols(),
           "line.wrap=" << line.wrap << " getCols()=" << getCols());
    ASSERT(_cursor.pos.col < getCols(),
           "_cursor.pos.cos=" << _cursor.pos.col << " getCols()=" << getCols());
    line.wrap = std::max<int16_t>(line.wrap, _cursor.pos.col + 1);
    ASSERT(line.wrap <= getCols(), "");

    if (_cursor.pos.col == getCols() - 1) {
        _cursor.wrapNext = true;
    }
    else {
        ++_cursor.pos.col;
    }

    damageCell();       // For the sake of the cursor.
}

void Buffer::backspace(bool autoWrap) {
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

void Buffer::forwardIndex(bool resetCol) {
    if (_cursor.pos.row == _marginEnd - 1) {
        addLine();
        moveCursor2(true, 0, !resetCol, 0);
    }
    else {
        moveCursor2(true, 1, !resetCol, 0);
    }
}

void Buffer::reverseIndex() {
    if (_cursor.pos.row == _marginBegin) {
        _cursor.wrapNext = false;
        insertLinesAt(_marginBegin, 1);
    }
    else {
        moveCursor2(true, -1, true, 0);
    }
}

void Buffer::setTab() {
    _tabs[_cursor.pos.col] = true;
}

void Buffer::unsetTab() {
    _tabs[_cursor.pos.col] = false;
}

void Buffer::clearTabs() {
    std::fill(_tabs.begin(), _tabs.end(), false);
}

void Buffer::moveCursor(Pos pos, bool marginRelative) {
    _cursor.wrapNext = false;
    damageCell();

    if (marginRelative) {
        // Convert pos to absolute and don't allow it below
        // the bottom margin.
        pos.row = std::min(pos.row + _marginBegin, _marginEnd - 1);
    }

    _cursor.pos = pos;

    _cursor.pos.row = clamp<int16_t>(_cursor.pos.row, 0, getRows() - 1);
    _cursor.pos.col = clamp<int16_t>(_cursor.pos.col, 0, getCols() - 1);

    damageCell();
}

void Buffer::moveCursor2(bool rowRelative, int16_t row,
                         bool colRelative, int16_t col) {
    _cursor.wrapNext = false;
    damageCell();

    if (rowRelative) {
        if (row > 0) {
            // If the cursor isn't already below the bottom margin then don't
            // allow it to go below.
            if (_cursor.pos.row < _marginEnd) {
                _cursor.pos.row = std::min<int16_t>(_cursor.pos.row + row, _marginEnd - 1);
            }
            else {
                _cursor.pos.row += row;
            }
        }
        else if (row < 0) {
            // If the cursor isn't already above the bottom margin then don't
            // allow it to go above.
            if (_cursor.pos.row >= _marginBegin) {
                _cursor.pos.row = std::max<int16_t>(_cursor.pos.row + row, _marginBegin);
            }
            else {
                _cursor.pos.row += row;
            }
        }
        else {
            // nothing to do
        }
    }
    else {
        _cursor.pos.row = row;
    }

    _cursor.pos.col = colRelative ? _cursor.pos.col + col : col;

    _cursor.pos.row = clamp<int16_t>(_cursor.pos.row, 0, getRows() - 1);
    _cursor.pos.col = clamp<int16_t>(_cursor.pos.col, 0, getCols() - 1);

    damageCell();
}

void Buffer::saveCursor() {
    _savedCursor.cursor          = _cursor;
    _savedCursor.cursor.wrapNext = false;      // XXX ??
    _savedCursor.charSub         = getCharSub(_cursor.charSet);
}

void Buffer::restoreCursor() {
    damageCell();

    _cursor = _savedCursor.cursor;
    if (_savedCursor.charSub) {
        _charSubs.set(_cursor.charSet, _savedCursor.charSub);
    }

    ASSERT(_cursor.pos.row < getRows(), "");
    ASSERT(_cursor.pos.col < getCols(), "");

    damageCell();
}

void Buffer::resizeClip(int16_t rows, int16_t cols) {
    ASSERT(rows > 0 && cols > 0, "");
    ASSERT(_cursor.pos.row >= 0 && _cursor.pos.row < getRows(), "");
    ASSERT(_cursor.pos.col >= 0 && _cursor.pos.col < getCols(), "");

    clearSelection();

    if (cols != getCols()) {
        for (auto & line : _active) {
            line.resize(cols);
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

    _savedCursor.cursor.pos.row = std::min<int16_t>(_savedCursor.cursor.pos.row, rows - 1);
    _savedCursor.cursor.pos.col = std::min<int16_t>(_savedCursor.cursor.pos.col, cols - 1);

    _damage.resize(rows);
    damageViewport(false);
}

void Buffer::resizeReflow(int16_t rows, int16_t cols) {
    ASSERT(rows > 0 && cols > 0, "");
    ASSERT(_cursor.pos.row >= 0 && _cursor.pos.row < getRows(), "");
    ASSERT(_cursor.pos.col >= 0 && _cursor.pos.col < getCols(), "");

    clearSelection();

    ASSERT(!_active.back().cont, "");

    // Remove blank lines from the back, stopping if we hit the cursor.
    while (getRows() > rows &&
           _cursor.pos.row < getRows() - 1 &&
           _active.back().isBlank())
    {
        _active.pop_back();

        // By popping lines off the back it is possible that the last remaining
        // line has 'cont' set. Just clobber it if so.
        _active.back().cont = false;
    }

    if (cols != getCols()) {
        bool     doneCursor     = false;
        uint32_t cursorTagIndex = 0;
        uint32_t cursorOffset   = 0;

        while (!_active.empty()) {
            auto s = _pending.size();       // Save this value before calling bump().

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

        _cols = cols;       // Must set before calling rebuildHistory().
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

    _savedCursor.cursor.pos.row = std::min<int16_t>(_savedCursor.cursor.pos.row, rows - 1);
    _savedCursor.cursor.pos.col = std::min<int16_t>(_savedCursor.cursor.pos.col, cols - 1);

    _damage.resize(rows);
    damageViewport(true);
}

void Buffer::tabForward(uint16_t count) {
    auto col = _cursor.pos.col;

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

    moveCursor2(true, 0, false, col);
}

void Buffer::tabBackward(uint16_t count) {
    auto col = _cursor.pos.col;

    while (count != 0) {
        if (col == 0) {
            break;
        }

        --col;

        if (_tabs[col]) {
            --count;
        }
    }

    moveCursor2(true, 0, false, col);
}

void Buffer::reset() {
    clear();
    resetMargins();
    resetTabs();
    resetCursor();
}

void Buffer::setMargins(int16_t begin, int16_t end) {
    if (end > begin) {
        _marginBegin = clamp<int16_t>(begin, 0, getRows() - 1);
        _marginEnd   = clamp<int16_t>(end,   begin + 1, getRows());
    }
    else {
        resetMargins();
    }
}

void Buffer::insertCells(uint16_t n) {
    ASSERT(n > 0, "n is not positive.");

    testClearSelection(APos(_cursor.pos, 0),
                       APos(Pos(_cursor.pos.row, getCols()), 0));

    n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);
    ASSERT(n > 0, "n is not positive.");

    auto & line = _active[_cursor.pos.row];
    line.wrap = std::min<int16_t>(getCols(), line.wrap + n);
    ASSERT(line.wrap <= getCols(), "");
    std::copy_backward(line.cells.begin() + _cursor.pos.col,
                       line.cells.end() - n,
                       line.cells.end());
    std::fill(line.cells.begin() + _cursor.pos.col,
              line.cells.begin() + _cursor.pos.col + n,
              Cell::blank(_cursor.style));

    damageColumns(_cursor.pos.col, getCols());

    ASSERT(!line.cont || line.wrap == _cols,
           "line.cont=" << std::boolalpha << line.cont <<
           ", line.wrap=" << line.wrap <<
           ", _cols=" << _cols);

    _cursor.wrapNext = false;
}

void Buffer::eraseCells(uint16_t n) {
    ASSERT(n > 0, "n is not positive.");

    testClearSelection(APos(_cursor.pos, 0),
                       APos(Pos(_cursor.pos.row, getCols()), 0));

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
              Cell::blank(_cursor.style));

    damageColumns(_cursor.pos.col, getCols());

    ASSERT(!line.cont || line.wrap == _cols,
           "line.cont=" << std::boolalpha << line.cont <<
           ", line.wrap=" << line.wrap <<
           ", _cols=" << _cols);

    _cursor.wrapNext = false;
}

void Buffer::blankCells(uint16_t n) {
    ASSERT(n > 0, "n is not positive.");

    testClearSelection(APos(_cursor.pos, 0),
                       APos(Pos(_cursor.pos.row, _cursor.pos.col + n), 0));

    _cursor.wrapNext = false;
    n = std::min<uint16_t>(n, getCols() - _cursor.pos.col);

    auto & line = _active[_cursor.pos.row];

    std::fill(line.cells.begin() + _cursor.pos.col,
              line.cells.begin() + _cursor.pos.col + n,
              Cell::blank(_cursor.style));

    damageColumns(_cursor.pos.col, _cursor.pos.col + n);
}

void Buffer::clearLine() {
    testClearSelection(APos(Pos(_cursor.pos.row, 0), 0),
                       APos(Pos(_cursor.pos.row, getCols()), 0));

    auto & line = _active[_cursor.pos.row];

    line.cont = false;
    line.wrap = 0;
    _cursor.wrapNext = false;
    std::fill(line.cells.begin(), line.cells.end(), Cell::blank(_cursor.style));
    damageColumns(0, getCols());

    ASSERT(!line.cont || line.wrap == _cols,
           "line.cont=" << std::boolalpha << line.cont <<
           ", line.wrap=" << line.wrap <<
           ", _cols=" << _cols);

    _cursor.wrapNext = false;
}

void Buffer::clearLineLeft() {
    testClearSelection(APos(Pos(_cursor.pos.row, _cursor.pos.col + 1), 0),
                       APos(Pos(_cursor.pos.row, 0), 0));

    auto & line = _active[_cursor.pos.row];

    _cursor.wrapNext = false;
    std::fill(line.cells.begin(),
              line.cells.begin() + _cursor.pos.col + 1,
              Cell::blank(_cursor.style));
    damageColumns(0, _cursor.pos.col + 1);

    ASSERT(!line.cont || line.wrap == _cols,
           "line.cont=" << std::boolalpha << line.cont <<
           ", line.wrap=" << line.wrap <<
           ", _cols=" << _cols);

    _cursor.wrapNext = false;
}

void Buffer::clearLineRight() {
    testClearSelection(APos(_cursor.pos, 0),
                       APos(Pos(_cursor.pos.row, getCols()), 0));

    auto & line = _active[_cursor.pos.row];

    line.cont = false;
    _cursor.wrapNext = false;
    line.wrap = std::min(line.wrap, _cursor.pos.col);
    ASSERT(line.wrap <= getCols(), "");
    std::fill(line.cells.begin() + _cursor.pos.col,
              line.cells.end(),
              Cell::blank(_cursor.style));
    damageColumns(_cursor.pos.col, line.cells.size());

    ASSERT(!line.cont || line.wrap == _cols,
           "line.cont=" << std::boolalpha << line.cont <<
           ", line.wrap=" << line.wrap <<
           ", _cols=" << _cols);

    _cursor.wrapNext = false;
}

void Buffer::clear() {
    clearSelection();

    for (auto & l : _active) { l.clear(_cursor.style); }
    damageActive();
    _cursor.wrapNext = false;
}

void Buffer::clearAbove() {
    testClearSelection(APos(),
                       APos(Pos(_cursor.pos.row, _cursor.pos.col + 1), 0));

    clearLineLeft();

    for (auto i = _active.begin(); i != _active.begin() + _cursor.pos.row; ++i) {
        i->clear(_cursor.style);
    }

    damageRows(0, _cursor.pos.row);
}

void Buffer::clearBelow() {
    testClearSelection(APos(_cursor.pos, 0),
                       APos(Pos(getRows() - 1, getCols()), 0));

    clearLineRight();

    for (auto i = _active.begin() + _cursor.pos.row + 1; i != _active.end(); ++i) {
        i->clear(_cursor.style);
    }

    damageRows(_cursor.pos.row + 1, getRows());
}

void Buffer::insertLines(uint16_t n) {
    if (_cursor.pos.row >= _marginBegin && _cursor.pos.row < _marginEnd) {
        _cursor.wrapNext = false;
        n = std::min<uint16_t>(n, _marginEnd - _cursor.pos.row);
        insertLinesAt(_cursor.pos.row, n);
    }
}

void Buffer::eraseLines(uint16_t n) {
    if (_cursor.pos.row >= _marginBegin && _cursor.pos.row < _marginEnd) {
        _cursor.wrapNext = false;
        n = std::min<uint16_t>(n, _marginEnd - _cursor.pos.row);
        eraseLinesAt(_cursor.pos.row, n);
    }
}

void Buffer::scrollUpMargins(uint16_t n) {
    _cursor.wrapNext = false;
    n = std::min<uint16_t>(n, _marginEnd - _marginBegin);
    eraseLinesAt(_marginBegin, n);
}

void Buffer::scrollDownMargins(uint16_t n) {
    _cursor.wrapNext = false;
    n = std::min<uint16_t>(n, _marginEnd - _marginBegin);
    insertLinesAt(_marginBegin, n);
}

void Buffer::damageViewport(bool scrollbar) {
    for (auto & d : _damage) {
        d.damageSet(0, getCols());
    }

    if (scrollbar) {
        _barDamage = true;
    }
}

void Buffer::damageActive() {
    damageRows(0, getRows());
}

void Buffer::testPattern() {
    for (auto & r : _active) {
        for (auto & c : r.cells) {
            c = Cell::ascii('E', _cursor.style);
        }
    }
    damageActive();
}

void Buffer::accumulateDamage(Region & damage) const {
    bool first = true;
    int16_t rowNum = 0;

    for (auto & d : _damage) {
        auto cB = d.begin;
        auto cE = d.end;

        if (UNLIKELY(cB != cE)) {
            if (UNLIKELY(first)) {
                damage.begin.row = rowNum;
                damage.end.row   = rowNum + 1;
                damage.begin.col = cB;
                damage.end.col   = cE;
                first    = false;
            }
            else {
                damage.end.row   = rowNum + 1;
                damage.begin.col = std::min(damage.begin.col, cB);
                damage.end.col   = std::max(damage.end.col, cE);
            }
        }

        ++rowNum;
    }
}

void Buffer::dispatch(bool reverse, I_Renderer & renderer) {
        dispatchBg(reverse, renderer);
        dispatchFg(reverse, renderer);

        if (_search) {
            dispatchSearch(reverse, renderer);
        }
        else {
            dispatchCursor(reverse, renderer);
        }

        resetDamage();
}

void Buffer::resetDamage() {
    for (auto & d : _damage) {
        d.reset();
    }
    _barDamage = false;
}

void Buffer::useCharSet(CharSet charSet) {
    _cursor.charSet = charSet;
}

void Buffer::setCharSub(CharSet charSet, const CharSub * charSub) {
    _charSubs.set(charSet, charSub);
}

const CharSub * Buffer::getCharSub(CharSet charSet) const {
    return _charSubs.get(charSet);
}

void Buffer::beginSearch(const std::string & pattern) {
    ASSERT(!_search, "Already searching.");
    _search = new Search(*this, pattern);
    _damage.back().damageAdd(0, getCols());

    auto & bufferIter = _search->iter;
    auto & allOffsets = _search->allOffsets;

    Regex regex(pattern);

    while (bufferIter.valid()) {
        auto paraIter = bufferIter.getParaIter();

        std::vector<uint8_t> para;

        while (paraIter.valid()) {
            auto & cell = paraIter.getCell();
            auto   seq  = cell.seq;

            std::copy(&seq.bytes[0],
                      &seq.bytes[utf8::leadLength(seq.lead())],
                      back_inserter(para));

            paraIter.moveForward();
        }

        para.push_back('\0');

        std::cout << &para.front() << std::endl;

        // PCRE_NOTEMPTY, PCRE_NO_UTF8_CHECK
        allOffsets = regex.matchAllOffsets(reinterpret_cast<const char *>(&para.front()),
                                           para.size());

        if (!allOffsets.empty()) {
            break;
        }

        bufferIter.moveBackward();
    }
}

const std::string & Buffer::getSearchPattern() const {
    ASSERT(_search, "Not searching.");
    return _search->pattern;
}

void Buffer::setSearchPattern(const std::string & pattern) {
    ASSERT(_search, "Not searching.");
    _search->pattern = pattern;
    // TODO
}

void Buffer::nextSearch() {
    ASSERT(_search, "Not searching.");
    NYI("");
}

void Buffer::prevSearch() {
    ASSERT(_search, "Not searching.");
    NYI("");
}

void Buffer::endSearch() {
    ASSERT(_search, "Not searching.");
    delete _search;
    _search = nullptr;
}

void Buffer::dumpTags(std::ostream & ost) const {
    ost << "BEGIN LOCAL TAGS" << std::endl;

    size_t            i = 0;
    std::vector<Cell> cells;

    for (auto tag : _tags) {
        ost << std::setw(6) << i << " "
            << std::setw(sizeof(I_Deduper::Tag) * 2) << std::setfill('0')
            << std::hex << std::uppercase << tag << ": "
            << std::setfill(' ') << std::dec << " \'";

        if (tag == I_Deduper::invalidTag()) {
            cells = _pending;
        }
        else {
            _deduper.lookup(tag, cells);
        }

        for (auto & cell : cells) {
            ost << cell.seq;
        }

        ost << "\'" << std::endl;

        ++i;
    }

    ost << "END LOCAL TAGS" << std::endl << std::endl;
}

void Buffer::dumpHistory(std::ostream & ost) const {
    ost << "BEGIN HISTORY" << std::endl;

    auto              flags = ost.flags();
    size_t            i = 0;
    std::vector<Cell> cells;

    for (auto & l : _history) {
        ost << std::setw(4) << i << " "
            << std::setw(4) << l.index - _lostTags << " "
            << std::setw(2) << l.seqnum << " \'";

        auto     tag    = _tags[l.index - _lostTags];
        uint32_t offset = l.seqnum * getCols();
        bool     cont;
        int16_t  wrap;

        if (tag == I_Deduper::invalidTag()) {
            cells = _pending;
            cont  = true;
            wrap  = getCols();
        }
        else {
            _deduper.lookupSegment(tag, offset, getCols(), cells, cont, wrap);
        }

        ASSERT(wrap <= getCols(), "");

        for (uint16_t o = 0; o != wrap; ++o) {
            auto c = offset + o;
            auto & cell = cells[c];
            ost << cell.seq;
        }

        ost << "\'" << std::endl;

        ++i;
    }

    ost.flags(flags);

    ost << "END HISTORY" << std::endl << std::endl;
}

void Buffer::dumpActive(std::ostream & ost) const {
    ost << "BEGIN ACTIVE" << std::endl;

    size_t i = 0;

    for (auto & l : _active) {
        ost << std::setw(2) << i << " "
            << (l.cont ? '\\' : '$') << " "
            << std::setw(3) << l.wrap << " \'";

        uint16_t col = 0;

        ost << CsiEsc::SGR(CsiEsc::StockSGR::UNDERLINE);
        for (; col != l.wrap; ++col) { ost << l.cells[col].seq; }
        ost << CsiEsc::SGR(CsiEsc::StockSGR::RESET_UNDERLINE);

        for (; col != getCols(); ++col) { ost << l.cells[col].seq; }

        ost << "\'" << std::endl;

        ++i;
    }

    ost << "END ACTIVE" << std::endl << std::endl;
}

void Buffer::dumpSelection(std::ostream & ost) const {
    ost << "BEGIN SELECTION" << std::endl;

    std::string text;
    if (getSelectedText(text)) {
        ost << text << std::endl;
    }

    ost << "END SELECTION" << std::endl << std::endl;
}

void Buffer::getLine(int32_t row, std::vector<Cell> & cells, bool & cont, int16_t & wrap) const {
    if (row < 0) {
        auto & hline = _history[_history.size() + row];
        auto   tag   = _tags[hline.index - _lostTags];

        uint32_t offset = hline.seqnum * getCols();

        if (UNLIKELY(tag == I_Deduper::invalidTag())) {
            wrap = std::min<uint32_t>(getCols(), _pending.size() - offset);
            std::copy(_pending.begin() + offset, _pending.begin() + offset + wrap, cells.begin());
            std::fill(cells.begin() + wrap, cells.end(), Cell::blank());
            cont = (offset + wrap != _pending.size());
        }
        else {
            _deduper.lookupSegment(tag, offset, getCols(), cells, cont, wrap);
        }
    }
    else {
        auto & aline = _active[row];

        std::copy(aline.cells.begin(), aline.cells.end(), cells.begin());
        wrap = aline.wrap;
        cont = aline.cont;
    }
}

void Buffer::dispatchBg(bool reverse, I_Renderer & renderer) const {
    APos selBegin, selEnd;
    auto selValid = normaliseSelection(selBegin, selEnd);

    // Declare this outside of the loop to avoid reallocation.
    std::vector<Cell> cells(getCols(), Cell::blank());

    for (int16_t row = 0; row != getRows(); ++row) {
        auto & damage = _damage[row];
        if (damage.begin == damage.end) { continue; }

        bool    cont;
        int16_t wrap;
        getLine(static_cast<int32_t>(row - _scrollOffset), cells, cont, wrap);

        auto bg0  = UColor::stock(UColor::Name::TEXT_BG);
        auto col0 = damage.begin;  // Accumulation start column.
        auto col1 = col0;

        for (; col1 != damage.end; ++col1) {
#if 0
            // Once we get past the wrap point all cells should be the same, so skip
            // to the last iteration. Unlike in dispatchFg() we must iterate over
            // the wrap character to handle selection correctly.
            if (col1 > wrap) { col1 = damage.end - 1; }
#endif

            auto   apos     = APos(row - _scrollOffset, col1);
            auto   selected = selValid && isCellSelected(apos, selBegin, selEnd, wrap);
            auto & cell     = cells[col1];
            auto & attrs    = cell.style.attrs;
            auto   swap     = XOR(reverse, attrs.get(Attr::INVERSE));
            auto   bg1      = bg0; // About to be overridden.

            if (UNLIKELY(selected)) {
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

            if (UNLIKELY(bg0 != bg1)) {
                if (col1 != col0) {
                    // flush run
                    renderer.bufferDrawBg(Pos(row, col0), col1 - col0, bg0);
                }

                col0 = col1;
                bg0  = bg1;
            }
        }

        // There may be an unterminated run to flush.
        if (col1 != col0) {
            renderer.bufferDrawBg(Pos(row, col0), col1 - col0, bg0);
        }
    }
}

void Buffer::dispatchFg(bool reverse, I_Renderer & renderer) const {
    APos selBegin, selEnd;
    auto selValid = normaliseSelection(selBegin, selEnd);

    // Declare these outside of the loop to avoid reallocation.
    std::vector<Cell>    cells(getCols(), Cell::blank());
    std::vector<uint8_t> run;               // Buffer for accumulating character runs.

    for (int16_t row = 0; row != getRows(); ++row) {
        auto & damage = _damage[row];
        if (damage.begin == damage.end) { continue; }

        bool    cont;
        int16_t wrap;
        getLine(static_cast<int32_t>(row - _scrollOffset), cells, cont, wrap);

        auto fg0    = UColor::stock(UColor::Name::TEXT_FG);
        auto attrs0 = AttrSet();
        auto col0   = damage.begin;   // Accumulation start column.
        auto col1   = col0;

        for (; col1 != damage.end; ++col1) {
#if 0
            // Once we get past the wrap point all cells will be blank,
            // so break out of the loop now.
            if (col1 >= wrap) { break; }
#endif

            auto   apos     = APos(row - _scrollOffset, col1);
            auto   selected = selValid && isCellSelected(apos, selBegin, selEnd, wrap);
            auto & cell     = cells[col1];
            auto   length   = utf8::leadLength(cell.seq.lead());
            auto & attrs1   = cell.style.attrs;
            auto   swap     = XOR(reverse, attrs1.get(Attr::INVERSE));
            auto   fg1      = fg0; // About to be overridden.

            if (UNLIKELY(selected)) {
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

            /* If the UTF-8 codepoint is more than one byte then terminate the run
             * to prevent the alignment being upset when the resulting glyph is
             * wider than the fixed width font.
             * Can we do this a better way?
             */
            if (UNLIKELY(length != utf8::Length::L1 ||
                         fg0    != fg1              ||
                         attrs0 != attrs1)) {
                if (col1 != col0) {
                    // flush run
                    auto size = run.size();
                    run.push_back(NUL);
                    renderer.bufferDrawFg(Pos(row, col0), col1 - col0,
                                          fg0, attrs0, &run.front(), size);
                    run.clear();
                }

                col0   = col1;
                fg0    = fg1;
                attrs0 = attrs1;
            }

            std::copy(cell.seq.bytes, cell.seq.bytes + length, std::back_inserter(run));
        }

        // There may be an unterminated run to flush.
        if (col1 != col0) {
            // flush run
            auto size = run.size();
            run.push_back(NUL);
            renderer.bufferDrawFg(Pos(row, col0), col1 - col0, fg0, attrs0, &run.front(), size);
            run.clear();
        }
    }
}

void Buffer::dispatchCursor(bool reverse, I_Renderer & renderer) const {
    APos selBegin, selEnd;
    bool selValid = normaliseSelection(selBegin, selEnd);

    auto r0 = _scrollOffset + static_cast<uint32_t>(_cursor.pos.row);
    if (r0 < static_cast<uint32_t>(getRows())) {
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
        renderer.bufferDrawCursor(Pos(r1, c1), fg, bg, attrs, &run.front(), size, _cursor.wrapNext);
    }
}

void Buffer::dispatchSearch(bool UNUSED(reverse), I_Renderer & renderer) const {
    auto row = getRows() - 1;

    auto str = "?" + _search->pattern;

    renderer.bufferDrawBg(Pos(row, 0), getCols(), UColor::stock(UColor::Name::TEXT_FG));
    renderer.bufferDrawFg(Pos(row, 0),
                          str.size(),
                          UColor::stock(UColor::Name::TEXT_BG),
                          AttrSet(),
                          reinterpret_cast<const uint8_t *>(str.data()),
                          str.size());
}

void Buffer::rebuildHistory() {
    _history.clear();

    uint32_t index = 0;

    for (auto tag : _tags) {
        auto length = (tag != I_Deduper::invalidTag() ?
                       _deduper.lookupLength(tag) :
                       _pending.size());

        uint16_t seqnum = 0;
        size_t   offset = 0;

        do {
            _history.push_back(HLine(index + _lostTags, seqnum));
            ++seqnum;
            offset += _cols;
        } while (offset < length);

        ++index;
    }

    _history.shrink_to_fit();
}

bool Buffer::isCellSelected(APos apos, APos begin, APos end, int16_t wrap) {
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

void Buffer::testClearSelection(APos begin, APos end) {
    APos selBegin, selEnd;
    if (normaliseSelection(selBegin, selEnd)) {
        if (selBegin < end && begin < selEnd) {
            clearSelection();
        }
    }
}

bool Buffer::normaliseSelection(APos & begin, APos & end) const {
    begin = _selectMark;
    end   = _selectDelim;

    if (begin == end) {
        return false;
    }
    else {
        if (end < begin) { std::swap(begin, end); }
        return true;
    }
}

void Buffer::insertLinesAt(int16_t row, uint16_t n) {
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
            _selectMark.row  += n;
            _selectDelim.row += n;
        }
    }

    _active.erase (_active.begin() + _marginEnd - n, _active.begin() + _marginEnd);
    _active.insert(_active.begin() + row, n, ALine(getCols(), _cursor.style));

    damageRows(row, _marginEnd);

    // We mustn't leave a line with 'cont' set when the continuation line
    // is gone. This can also cause _active.back().cont to be true, violating
    // our invariant, if _marginEnd == getRows().
    _active[_marginEnd - 1].cont = false;
}

void Buffer::eraseLinesAt(int16_t row, uint16_t n) {
    ASSERT(row >= _marginBegin && row < _marginEnd, "");
    ASSERT(row + n <= _marginEnd, "row=" << row << ", n=" << n <<
           ", margin-end=" << _marginEnd);

    APos begin, end;
    if (normaliseSelection(begin, end)) {
        if (begin.row < row + n && end.row >= row) {
            // Clear the selection because it spans or will collide with the
            // erasure point.
            clearSelection();
        }
        else if (begin.row < _marginEnd && end.row >= _marginEnd) {
            // Clear the selection because it spans the insertion point.
            clearSelection();
        }
        else {
            _selectMark.row  -= n;
            _selectDelim.row -= n;
        }
    }

    _active.erase (_active.begin() + row, _active.begin() + row + n);
    _active.insert(_active.begin() + _marginEnd - n, n, ALine(getCols(), _cursor.style));

    damageRows(row, _marginEnd);

    ASSERT(!_active.back().cont, "");
}

bool Buffer::marginsSet() const {
    return _marginBegin != 0 || _marginEnd != getRows();
}

void Buffer::damageCell() {
    auto damageRow = _scrollOffset + static_cast<uint32_t>(_cursor.pos.row);

    if (damageRow < static_cast<uint32_t>(getRows())) {
        _damage[damageRow].damageAdd(_cursor.pos.col, _cursor.pos.col + 1);
    }
}

void Buffer::damageColumns(int16_t begin, int16_t end) {
    ASSERT(begin <= end, "");
    ASSERT(begin >= 0, "");
    ASSERT(end   <= getCols(), "");

    auto damageRow = _scrollOffset + static_cast<uint32_t>(_cursor.pos.row);

    if (damageRow < static_cast<uint32_t>(getRows())) {
        _damage[damageRow].damageAdd(begin, end);
    }
}

void Buffer::damageRows(int16_t begin, int16_t end) {
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

void Buffer::damageSelection() {
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

void Buffer::addLine() {
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

        _active.emplace_back(getCols());

        APos begin, end;
        if (normaliseSelection(begin, end)) {
            if (begin.row == -static_cast<int32_t>(_history.size())) {
                clearSelection();
            }
            else {
                --_selectMark.row;
                --_selectDelim.row;
            }
        }
        else {
            if (_selectMark.row > -static_cast<int32_t>(_history.size())) {
                --_selectMark.row;
                --_selectDelim.row;
            }
        }

        damageViewport(true);
    }
}

void Buffer::bump() {
    auto & aline = _active.front();

    ASSERT(!aline.cont || aline.wrap == _cols,
           "aline.cont=" << std::boolalpha << aline.cont <<
           ", aline.wrap=" << aline.wrap <<
           ", _cols=" << _cols);

    auto & cells = aline.cells;
    auto   cont  = aline.cont;
    auto   wrap  = aline.wrap;

    if (_pending.empty()) {
        // This line is not a continuation of a previous line.
        ASSERT(_tags.empty() || _tags.back() != I_Deduper::invalidTag(), "");
        ASSERT(_history.empty() || _history.back().index - _lostTags == _tags.size() - 1, "");

        if (cont) {
            // This line is continued on the next line so it can't be stored
            // for dedupe yet.
            _pending = std::move(cells);
            ASSERT(cells.empty(), "Not stolen by move constructor?");
            _tags.push_back(I_Deduper::invalidTag());
        }
        else {
            // This line is completely standalone. Immediately dedupe it.
            ASSERT(static_cast<size_t>(wrap) <= cells.size(), "");
            cells.erase(cells.begin() + wrap, cells.end());
            auto tag = _deduper.store(cells);
            ASSERT(tag != I_Deduper::invalidTag(), "");
            _tags.push_back(tag);
        }

        _history.push_back(HLine(_tags.size() + _lostTags - 1, 0));
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

        _pending.resize(oldSize + wrap, Cell::blank());
        std::copy(cells.begin(), cells.begin() + wrap, _pending.begin() + oldSize);
        _history.push_back(HLine(_tags.size() + _lostTags - 1, _history.back().seqnum + 1));

        if (!cont) {
            // This line is not itself continued.
            // Store _pending and the tag.
            auto tag = _deduper.store(_pending);
            ASSERT(tag != I_Deduper::invalidTag(), "");
            _pending.clear();
            ASSERT(_tags.back() == I_Deduper::invalidTag(), "");
            _tags.back() = tag;
        }
    }

    ASSERT(!_history.empty() && _history.back().index - _lostTags == _tags.size() - 1, "");

    _active.pop_front();        // This invalidates 'aline'.
}

void Buffer::unbump() {
    ASSERT(!_tags.empty(), "");
    ASSERT(!_history.empty(), "");

    auto & hline = _history.back();
    ASSERT(hline.index - _lostTags == _tags.size() - 1, "");

    bool cont;

    if (_pending.empty()) {
        cont = false;
        auto tag = _tags.back();
        ASSERT(tag != I_Deduper::invalidTag(), "");
        _deduper.lookup(tag, _pending);
        _deduper.remove(tag);
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

    _active.emplace_front(cells, cont, cells.size(), _cols);
    ASSERT(cells.empty(), "Not stolen by move constructor?");
    ASSERT(_active.front().wrap <= _cols, "");

    _history.pop_back();

    if (_history.empty() || _history.back().index - _lostTags != _tags.size() - 1) {
        _tags.pop_back();
        ASSERT(_pending.empty(), "");
    }
}

void Buffer::enforceHistoryLimit() {
    while (_tags.size() > _historyLimit) {
        while (!_history.empty() && _history.front().index == _lostTags) {
            if (_scrollOffset == _history.size()) { --_scrollOffset; }
            _history.pop_front();
        }

        _deduper.remove(_tags.front());
        _tags.pop_front();
        ++_lostTags;
    }

    APos begin, end;
    if (normaliseSelection(begin, end)) {
        if (static_cast<int32_t>(_history.size()) + begin.row < 0) {
            clearSelection();
        }
    }
}
