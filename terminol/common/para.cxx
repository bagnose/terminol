// vi:noai:sw=4
// Copyright © 2015 David Bryant

#include "terminol/common/para.hxx"

#include <iomanip>

Para::Para() : _styles(), _string(), _indices({0}) {}

Para::Para(const std::vector<Style>   & styles,
           const std::vector<uint8_t> & string) :
    _styles(styles),
    _string(string),
    _indices()
{
    _indices.reserve(_styles.size() + 1);

    int16_t index = 0;
    for (auto iter = string.begin(); iter != string.end(); /* */) {
        _indices.push_back(index);
        uint8_t length = utf8::leadLength(*iter);
        iter  += length;
        index += length;
    }
    _indices.push_back(index);

    ASSERT(_indices.size() == _styles.size() + 1, "");
}

const uint8_t * Para::getStringAtOffset(uint32_t offset) const {
    offset = std::min(offset, getLength());
    uint32_t index = _indices[offset];
    // Note, index can be out-of-range, in which case the result must not
    // be dereferenced.
    return &_string[index];
}

void Para::setCell(uint32_t offset, const Cell & cell) {
    expand(offset + 1);

    _styles[offset] = cell.style;

    auto   index       = _indices[offset];
    auto   newLength   = utf8::leadLength(cell.seq.lead());
    auto   oldLength   = utf8::leadLength(_string[index]);
    int8_t deltaLength = newLength - oldLength;

    if (deltaLength > 0) {
        _string.resize(_string.size() + deltaLength);
        std::copy_backward(_string.begin() + index + oldLength,
                           _string.end() - deltaLength,
                           _string.end());
    }
    else if (deltaLength < 0) {
        std::copy(_string.begin() + index + oldLength,
                  _string.end(),
                  _string.begin() + index + newLength);
        _string.resize(_string.size() + deltaLength);
    }

    std::copy(&cell.seq.bytes[0], &cell.seq.bytes[0] + newLength, &_string[index]);

    if (deltaLength != 0) {
        for (auto iter = _indices.begin() + offset + 1; iter != _indices.end(); ++iter) {
            *iter += deltaLength;
        }
    }
}

void Para::insertCell(uint32_t offset, uint32_t end, const Cell & cell) {
    auto index     = _indices[offset];
    auto newLength = utf8::leadLength(cell.seq.lead());

    // Insert at 'offset'.
    _styles.insert(_styles.begin() + offset, cell.style);
    _string.insert(_string.begin() + index, &cell.seq.bytes[0], &cell.seq.bytes[newLength]);
    _indices.insert(_indices.begin() + offset, _indices[offset]);
    for (auto iter = _indices.begin() + offset + 1; iter != _indices.end(); ++iter) {
        *iter += newLength;
    }

    // Erase at 'end'.
    _string.erase(_string.begin() + _indices[end], _string.begin() + _indices[end + 1]);
    _styles.erase(_styles.begin() + end);
    auto oldLength = _indices[end + 1] - _indices[end];
    for (auto iter = _indices.begin() + end + 1; iter != _indices.end(); ++iter) {
        *iter -= oldLength;
    }
    _indices.erase(_indices.begin() + end);
}

Cell Para::getCell(uint32_t offset) const {
    if (offset >= getLength()) {
        return Cell::blank();
    }
    else {
        Cell cell = Cell::blank();

        cell.style = _styles[offset];

        auto indexBegin = _indices[offset];
        auto indexEnd   = _indices[offset + 1];

        std::copy(&_string[indexBegin], &_string[indexEnd], cell.seq.bytes);

        return cell;
    }
}

void Para::truncate(uint32_t length) {
    if (length < getLength()) {
        _string.erase(_string.begin() + _indices[length], _string.end());
        _indices.erase(_indices.begin() + length + 1, _indices.end());
        _styles.erase(_styles.begin() + length, _styles.end());
    }
}

void Para::dump(std::ostream & ost, bool decorate) const {
    if (decorate) {
        ost << std::setw(3) << "len=" << getLength() << " '";
    }

    ost << std::string(_string.begin(), _string.end());

    if (decorate) {
        ost << "'";
    }

    ost << std::endl;
}

void Para::expand(uint32_t newSize) {
    auto oldSize = getLength();

    if (newSize <= oldSize) {
        return;
    }

    _styles.resize(newSize);

    _indices.resize(newSize + 1);
    auto index = _indices[oldSize];
    for (auto iter = _indices.begin() + oldSize + 1; iter != _indices.end(); ++iter) {
        *iter = ++index;
    }

    _string.resize(_string.size() + newSize - oldSize, ' ');
}
