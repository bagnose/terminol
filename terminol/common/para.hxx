// vi:noai:sw=4
// Copyright © 2017 David Bryant

#ifndef COMMON__PARA__HXX
#define COMMON__PARA__HXX

#include "terminol/common/data_types.hxx"

#include <iostream>

class Para {
    std::vector<Style>   _styles;
    std::vector<uint8_t> _string;
    std::vector<int32_t> _indices; // Index of each code point in _string.

public:
    Para();

    // Pass-by-value because we are taking a copy.
    Para(std::vector<Style> styles, std::vector<uint8_t> string);

    const std::vector<Style> &   getStyles() const { return _styles; }
    const std::vector<uint8_t> & getString() const { return _string; }

    uint32_t getLength() const { return static_cast<uint32_t>(_styles.size()); }

    Style getStyle(uint32_t offset) const {
        ASSERT(offset < getLength(), );
        return _styles[offset];
    }

    const uint8_t * getStringAtOffset(uint32_t offset) const;

    void setCell(uint32_t offset, const Cell & cell);

    void insertCell(uint32_t offset, uint32_t end, const Cell & cell);

    Cell getCell(uint32_t offset) const;

    // XXX should truncate be allowed to lengthen the para? If so, call it setLength().
    void truncate(uint32_t length);

    void dump(std::ostream & ost, bool decorate) const;

private:
    void expand(uint32_t newSize);
};

#endif // COMMON__PARA__HXX
