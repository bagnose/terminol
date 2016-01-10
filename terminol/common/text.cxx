// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/text.hxx"
#include "terminol/support/math.hxx"

#include <iomanip>

Text::Text(I_Repository & repository,
           ParaCache    & paraCache,
           int16_t        rows,
           int16_t        cols,
           uint32_t       historyLimit) :
    //
    _repository(repository),
    _paraCache(paraCache),
    //
    _historyTags(),
    _poppedHistoryTags(0),
    _historyLines(),
    //
    _currentParas(rows),
    _poppedCurrentParas(0),
    _straddlingLines(0),
    _currentLines(),        // Generated below.
    //
    _cols(cols)/*,
    _historyTagLimit(historyLimit)*/
{
    ASSERT(rows > 0 && cols > 0, "Rows and cols must be positive.");
    for (int16_t r = 0; r != rows; ++r) {
        _currentLines.emplace_back(r, 0, false);
    }
}

auto Text::begin() const -> Marker {
    Marker marker;

    marker._valid = true;
    marker._row   = 0;        // XXX

    if (_historyTags.empty()) {
        marker._current = true;
        marker._index   = 0;
    }
    else {
        marker._current = false;
        marker._index   = 0;
    }

    return marker;
}

auto Text::end() const -> Marker {
    Marker marker;

    marker._valid   = true;
    marker._row     = getRows();
    marker._current = true;
    marker._index   = _currentParas.size();

    return marker;
}

Text::~Text() {
}

int16_t Text::getRows() const {
    return static_cast<int16_t>(_currentLines.size() - _straddlingLines);
}

int16_t Text::getCols() const {
    return _cols;
}

void Text::setCell(int16_t row, int16_t col, const Cell & cell) {
    ASSERT(row < getRows() && col < getCols(), "");

    auto   & line    = _currentLines[row + _straddlingLines];
    auto   & para    = _currentParas[line.getIndex() - _poppedCurrentParas];
    uint32_t baseCol = _cols * line.getSeqnum();
    para.setCell(baseCol + col, cell);
}

void Text::insertCell(int16_t row, int16_t col, const Cell & cell) {
    ASSERT(row < getRows() && col < getCols(), "");
    auto   & line    = _currentLines[row + _straddlingLines];
    auto   & para    = _currentParas[line.getIndex() - _poppedCurrentParas];
    uint32_t baseCol = _cols * line.getSeqnum();
    para.insertCell(baseCol + col, baseCol + getCols(), cell);
}

Cell Text::getCell(int16_t row, int16_t col) const {
    auto   & line    = _currentLines[row + _straddlingLines];
    auto   & para    = _currentParas[line.getIndex() - _poppedCurrentParas];
    uint32_t baseCol = _cols * line.getSeqnum();
    return para.getCell(baseCol + col);
}

void Text::scrollDown(int16_t rowBegin, int16_t rowEnd, int16_t n) {
    ASSERT(rowBegin >= 0 && rowEnd <= getRows(), "");
    ASSERT(rowBegin + n <= rowEnd, "");
    ASSERT(n > 0, "");

    // The text moves down (the viewport moves up).

    if (rowBegin > 0) {
        makeUncontinued(rowBegin - 1);
    }

    if (rowEnd < getRows() - 1) {
        makeUncontinued(rowEnd - 1);
    }

    for (int16_t i = 0; i != n; ++i) {
        if (rowBegin < rowEnd - 1) {
            makeUncontinued(rowEnd - 2);
        }

        // Erase the paragraph that belongs to the last line.
        {
            auto & line = _currentLines[_straddlingLines + rowEnd - 1];
            _currentParas.erase(_currentParas.begin() + line.getIndex());
        }

        // Erase the last line.
        _currentLines.erase (_currentLines.begin() + _straddlingLines + rowEnd - 1);

        uint32_t index = 0;

        if (rowBegin > 0) {
        }

        // Insert a line at the beginning and a corresponding paragraph.
        _currentLines.insert(_currentLines.begin() + _straddlingLines + rowBegin,
                             Line(index, 0, false));
        _currentParas.insert(_currentParas.begin() + index, Para());

    }
}

void Text::scrollUp(int16_t rowBegin, int16_t rowEnd, int16_t n) {
    ASSERT(rowBegin >= 0 && rowEnd <= getRows(), "");
    ASSERT(rowBegin + n <= rowEnd, "");
    ASSERT(n > 0, "");

    // The text moves up (the viewport moves down).

    if (rowBegin > 0) {
        makeUncontinued(rowBegin - 1);
    }

    if (rowEnd < getRows() - 1) {
        makeUncontinued(rowEnd - 1);
    }

    for (int16_t i = 0; i != n; ++i) {
        makeUncontinued(rowBegin);

        // Erase the paragraph that belongs to the first line.
        {
            auto & line = _currentLines[_straddlingLines + rowBegin];
            _currentParas.erase(_currentParas.begin() + line.getIndex());
        }

        // Erase the first line.
        _currentLines.erase(_currentLines.begin() + _straddlingLines + rowBegin);

        uint32_t index = 0;

        for (auto iter = _currentLines.begin() + _straddlingLines + rowBegin;
             iter != _currentLines.begin() + _straddlingLines + rowEnd - 1;
             ++iter)
        {
            auto & line = *iter;
            index = line.getIndex();
            line.decrementIndex();
        }

        // Insert a line at the end and a corresponding paragraph.
        _currentLines.insert(_currentLines.begin() + _straddlingLines + rowEnd - 1,
                             Line(index, 0, false));
        _currentParas.insert(_currentParas.begin() + index, Para());
    }

}

void Text::addLine(bool continuation) {
    // Add the new line to the deque of current lines.

    if (continuation) {
        auto & lastLine = _currentLines.back();
        lastLine.setContinued(true);
        _currentLines.push_back(Line(lastLine.getIndex(), lastLine.getSeqnum() + 1, false));
    }
    else {
        _currentLines.push_back(Line(_currentParas.size() + _poppedCurrentParas, 0, false));
        _currentParas.emplace_back();
    }

    ++_straddlingLines;

    cleanStraddling();
}

void Text::makeContinued(int16_t row) {
    ASSERT(0 <= row && row < getRows() - 1, "Row out of range.");

    if (!_currentLines[row + _straddlingLines].isContinued())
    {
        // This line isn't continued, so there is something to do.

        if (row < getRows() - 2 &&
            _currentLines[row + 1 + _straddlingLines].isContinued()) {
            // The next line is continued, make it uncontinued.
            makeUncontinued(row + 1);
        }

        auto & thisLine = _currentLines[row + _straddlingLines];

        thisLine.setContinued(true);

        auto & nextLine = _currentLines[row + _straddlingLines + 1];
        ASSERT(!nextLine.isContinued(), "");

        auto & thisPara = _currentParas[thisLine.getIndex()];
        auto & nextPara = _currentParas[nextLine.getIndex()];

        for (uint32_t p0 = 0; p0 != nextPara.getLength(); ++p0) {
            auto cell = nextPara.getCell(p0);
            uint32_t p1 = p0 + (thisLine.getSeqnum() + 1) * getCols();
            thisPara.setCell(p1, cell);
        }

        for (auto iter = _currentLines.begin() + _straddlingLines + row + 2;
             iter != _currentLines.end(); ++iter)
        {
            auto & postLine = *iter;
            postLine.decrementIndex();
        }

        _currentParas.erase(_currentParas.begin() + nextLine.getIndex());   // Invalidates para refs.
        nextLine.setIndexSeqnum(thisLine.getIndex(), thisLine.getSeqnum() + 1);
    }
}

void Text::makeUncontinued(int16_t row) {
    ASSERT(0 <= row && row < getRows() - 1, "Row out of range.");

    if (_currentLines[row + _straddlingLines].isContinued()) {
        // This line is continued, so there is something to do.

        if (row < getRows() - 2 &&
            _currentLines[row + 1 + _straddlingLines].isContinued()) {
            // The next line is also continued, make it uncontinued.
            makeUncontinued(row + 1);
        }

        auto & thisLine = _currentLines[row + _straddlingLines];

        _currentParas.insert(_currentParas.begin() + thisLine.getIndex() + 1, Para());

        auto & nextLine = _currentLines[row + _straddlingLines + 1];
        ASSERT(!nextLine.isContinued(), "");
        nextLine.setIndexSeqnum(thisLine.getIndex() + 1, 0);

        auto & thisPara = _currentParas[thisLine.getIndex()];
        auto & nextPara = _currentParas[nextLine.getIndex()];

        for (uint32_t p0 = (thisLine.getSeqnum() + 1) * getCols();
             p0 < thisPara.getLength(); ++p0)
        {
            auto cell = thisPara.getCell(p0);
            uint32_t p1 = p0 - (thisLine.getSeqnum() + 1) * getCols();
            nextPara.setCell(p1, cell);
        }

        for (auto iter = _currentLines.begin() + _straddlingLines + row + 2;
             iter != _currentLines.end(); ++iter)
        {
            auto & postLine = *iter;
            postLine.incrementIndex();
        }

        thisPara.truncate((thisLine.getSeqnum() + 1) * getCols());
        thisLine.setContinued(false);
    }
}

void Text::resizeClip(int16_t rows, int16_t cols, const std::vector<Marker *> & points) {
    ASSERT(rows > 0 && cols > 0, "");

    if (cols != getCols()) {
    }
}

void Text::resizeReflow(int16_t rows, int16_t cols, const std::vector<Marker *> & points) {
}

void Text::dumpHistory(std::ostream & ost, bool UNUSED(decorate)) const {
    ost << "WIP" << std::endl;
}

void Text::dumpCurrent(std::ostream & ost, bool decorate) const {
    int32_t userRow = 0;

    for (auto & line : _currentLines) {
        auto   & para     = _currentParas[line.getIndex() - _poppedCurrentParas];
        uint32_t baseCol  = _cols * line.getSeqnum();
        auto     strBegin = para.getStringAtOffset(baseCol);
        auto     strEnd   = para.getStringAtOffset(baseCol + _cols);

        if (decorate) {
            ost << std::setw(3)
                << (userRow - static_cast<int32_t>(_straddlingLines))
                << " '";
        }

        ost << std::string(strBegin, strEnd);

        if (decorate) {
            ost << "'";

            if (line.isContinued()) {
                ost << " \\";
            }
        }

        ost << std::endl;

        ++userRow;
    }

    ost << std::endl;
}

void Text::dumpDetail(std::ostream & ost, bool UNUSED(blah)) {
    ost << "<<<<<" << std::endl;

    int32_t userRow = 0;
    ost << "history paras {" << std::endl;
    for (auto & tag : _historyTags) {
        ost << std::setw(3) << userRow << " ";
        auto & para = _paraCache.checkout(tag);
        para.dump(ost, true);
        _paraCache.checkin(tag);
        ++userRow;
    }
    ost << "} history tags" << std::endl;

    userRow = 0;
    ost << "history lines {" << std::endl;
    for (auto & line : _historyLines) {
        ost << std::setw(3) << userRow << " ";
        line.dump(ost, _poppedHistoryTags);
        ++userRow;
    }
    ost << "} history lines" << std::endl;

    userRow = 0;
    ost << "current paras {" << std::endl;
    for (auto & para : _currentParas) {
        ost << std::setw(3) << userRow << " ";
        para.dump(ost, true);
        ++userRow;
    }
    ost << "} current tags" << std::endl;

    userRow = 0;
    ost << "current lines {" << std::endl;
    for (auto & line : _currentLines) {
        ost << std::setw(3) << userRow - static_cast<int32_t>(_straddlingLines) << " ";
        line.dump(ost, _poppedCurrentParas);
        ++userRow;
    }
    ost << "} current lines" << std::endl;

    ost << ">>>>>" << std::endl;
}

auto Text::rfind(const Regex & regex, Marker & marker, bool & ongoing) -> std::vector<Match> {
    ASSERT(marker._valid, "Invalid marker.");

    std::vector<Match> matches;

    if (marker._index == 0) {
        if (marker._current && !_historyTags.empty()) {
            marker._current = false;
            marker._index   = _historyTags.size() - 1;
        }
        else {
            ongoing = false;
            return matches;
        }
    }
    else {
        --marker._index;
    }

    const Para * paraPtr = nullptr;

    if (marker._current) {
        paraPtr = &_currentParas[marker._index];
    }
    else {
        // checkout
        auto tag = _historyTags[marker._index];
        paraPtr = &_paraCache.checkout(tag);
    }

    auto & para = *paraPtr;

    if (para.getLength() == 0) {
        --marker._row;
    }
    else {
        marker._row -= divideRoundUp(para.getLength(), getCols());
    }

#if DEBUG
    auto & line = marker._current ? _currentLines[marker._row] : _currentLines[marker._row];
    ASSERT(line.getIndex()  == marker._index, "Indices don't match.");
    ASSERT(line.getSeqnum() == 0, "Expected zero seqnum.");
#endif

    auto substrs = regex.matchOffsets(para.getString());

    for (auto iter = substrs.rbegin(); iter != substrs.rend(); ++iter) {
        auto & substr = *iter;

        auto b = static_cast<uint32_t>(substr.first);
        auto e = static_cast<uint32_t>(substr.last);

        Match match;
        match._valid       = true;
        match._row         = marker._row + b / getCols();
        match._col         = b % getCols();
        match._current     = marker._current;
        match._index       = marker._index;
        match._offsetBegin = b;
        match._offsetEnd   = e;

        matches.push_back(match);
    }

    if (!marker._current) {
        // checkin
        auto tag = _historyTags[marker._index];
        _paraCache.checkin(tag);
    }

    ongoing = true;

    return matches;
}

void Text::cleanStraddling() {
    if (_straddlingLines > 0 && !_currentLines[_straddlingLines - 1].isContinued()) {
        auto tag = _repository.store(
            I_Repository::Entry(_currentParas.front().getStyles(), _currentParas.front().getString()));

        for (uint32_t seqnum = 0; seqnum != _straddlingLines; ++seqnum) {
            uint32_t index = _historyTags.size() + _poppedHistoryTags;
            bool continued = seqnum != _straddlingLines - 1;
            _historyLines.push_back(Line(index, seqnum, continued));
            _currentLines.pop_front();
        }

        _historyTags.push_back(tag);

        _currentParas.pop_front();
        ++_poppedCurrentParas;

        _straddlingLines = 0;
    }

}

#if 0
void Text::validate(Point & point) const {
    if (point._valid) { return; }

    int32_t row = point._row + _straddlingLines;

    if (row < 0) {
        // historical
        NYI("validate historical point");

        auto & line = _historyLines[_historyLines.size() + row];

        point._current = false;
        point._index   = line.getIndex() - _poppedHistoryTags;
        point._offset  = point._col + (line.getSeqnum() * getCols());
    }
    else {
        // current
        auto & line = _currentLines[row];

        point._current = true;
        point._index   = line.getIndex() - _poppedCurrentParas;
        point._offset  = point._col + (line.getSeqnum() * getCols());
    }

    point._valid = true;
}
#endif
