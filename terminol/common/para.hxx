// vi:noai:sw=4
// Copyright © 2015 David Bryant

#ifndef COMMON__PARA__HXX
#define COMMON__PARA__HXX

#include "terminol/common/data_types.hxx"

#include <iostream>

class Para {
    std::vector<Style>   _styles;
    std::vector<uint8_t> _string;
    std::vector<int16_t> _indices;  // Indices of each code point in _string.

    void expand(uint32_t newSize);

public:
    Para();

    Para(const std::vector<Style>   & styles,
         const std::vector<uint8_t> & string);

    const std::vector<Style>   & getStyles() const { return _styles; }
    const std::vector<uint8_t> & getString() const { return _string; }

    uint32_t getLength() const { return static_cast<uint32_t>(_styles.size()); }

    Style getStyle(uint32_t offset) const {
        ASSERT(offset < getLength(), "");
        return _styles[offset];
    };

    const uint8_t * getStringAtOffset(uint32_t offset) const;

    void setCell(uint32_t offset, const Cell & cell);

    void insertCell(uint32_t UNUSED(offset), uint32_t UNUSED(end), const Cell & UNUSED(cell));

    Cell getCell(uint32_t offset) const;

    // XXX should truncate be allowed to lengthen the para? If so, call it setLength().
    void truncate(uint32_t length);

    void dump(std::ostream & ost, bool decorate) const;
};

#endif // COMMON__PARA__HXX
