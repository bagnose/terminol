// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__ASCII__HXX
#define COMMON__ASCII__HXX

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

const uint8_t NUL   = '\x00';  // '\0'
const uint8_t SOH   = '\x01';
const uint8_t STX   = '\x02';
const uint8_t ETX   = '\x03';
const uint8_t EOT   = '\x04';
const uint8_t ENQ   = '\x05';
const uint8_t ACK   = '\x06';
const uint8_t BEL   = '\x07';  // '\a'
const uint8_t BS    = '\x08';  // '\b'
const uint8_t HT    = '\x09';  // '\t'
const uint8_t LF    = '\x0A';  // '\n'
const uint8_t VT    = '\x0B';  // '\v'
const uint8_t FF    = '\x0C';  // '\f'
const uint8_t CR    = '\x0D';  // '\r'
const uint8_t SO    = '\x0E';
const uint8_t SI    = '\x0F';
const uint8_t DLE   = '\x10';
const uint8_t DC1   = '\x11';
const uint8_t DC2   = '\x12';
const uint8_t DC3   = '\x13';
const uint8_t DC4   = '\x14';
const uint8_t NAK   = '\x15';
const uint8_t SYN   = '\x16';
const uint8_t ETB   = '\x17';
const uint8_t CAN   = '\x18';
const uint8_t EM    = '\x19';
const uint8_t SUB   = '\x1A';
const uint8_t ESC   = '\x1B';  // '\E'
const uint8_t FS    = '\x1C';
const uint8_t GS    = '\x1D';
const uint8_t RS    = '\x1E';
const uint8_t US    = '\x1F';
const uint8_t SPACE = '\x20';

// '\x21'..'\x40': '!'..'@'
// '\x41'..'\x5A': 'A'..'Z'
//
// '\x61'..'\x7A': 'a'..'z'

const uint8_t DEL   = '\x7F';

// Streaming helper.
struct Char {
    explicit Char(uint8_t c_) noexcept : c(c_) {}
    uint8_t c;
};

inline std::ostream & operator << (std::ostream & ost, Char ch) {
    switch (ch.c) {
        case NUL:
            return ost << "\\0";
        case BEL:
            return ost << "\\a";
        case BS:
            return ost << "\\b";
        case HT:
            return ost << "\\t";
        case LF:
            return ost << "\\n";
        case VT:
            return ost << "\\v";
        case FF:
            return ost << "\\f";
        case CR:
            return ost << "\\r";
        case ESC:
            return ost << "\\E";
        default:
            return ost << ch.c;
    }
}

#endif // COMMON__ASCII__HXX
