// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef COMMON__ASCII__HXX
#define COMMON__ASCII__HXX

#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

constexpr uint8_t NUL   = '\x00'; // '\0'
constexpr uint8_t SOH   = '\x01';
constexpr uint8_t STX   = '\x02';
constexpr uint8_t ETX   = '\x03';
constexpr uint8_t EOT   = '\x04';
constexpr uint8_t ENQ   = '\x05';
constexpr uint8_t ACK   = '\x06';
constexpr uint8_t BEL   = '\x07'; // '\a'
constexpr uint8_t BS    = '\x08'; // '\b'
constexpr uint8_t HT    = '\x09'; // '\t'
constexpr uint8_t LF    = '\x0A'; // '\n'
constexpr uint8_t VT    = '\x0B'; // '\v'
constexpr uint8_t FF    = '\x0C'; // '\f'
constexpr uint8_t CR    = '\x0D'; // '\r'
constexpr uint8_t SO    = '\x0E';
constexpr uint8_t SI    = '\x0F';
constexpr uint8_t DLE   = '\x10';
constexpr uint8_t DC1   = '\x11';
constexpr uint8_t DC2   = '\x12';
constexpr uint8_t DC3   = '\x13';
constexpr uint8_t DC4   = '\x14';
constexpr uint8_t NAK   = '\x15';
constexpr uint8_t SYN   = '\x16';
constexpr uint8_t ETB   = '\x17';
constexpr uint8_t CAN   = '\x18';
constexpr uint8_t EM    = '\x19';
constexpr uint8_t SUB   = '\x1A';
constexpr uint8_t ESC   = '\x1B'; // '\E'
constexpr uint8_t FS    = '\x1C';
constexpr uint8_t GS    = '\x1D';
constexpr uint8_t RS    = '\x1E';
constexpr uint8_t US    = '\x1F';
constexpr uint8_t SPACE = '\x20';

// '\x21'..'\x40': '!'..'@'
// '\x41'..'\x5A': 'A'..'Z'
//
// '\x61'..'\x7A': 'a'..'z'

constexpr uint8_t DEL = '\x7F';

// Streaming helper.
struct Char {
    uint8_t c = 0;
};

inline std::ostream & operator<<(std::ostream & ost, Char ch) {
    switch (ch.c) {
    case NUL: return ost << "\\0";
    case BEL: return ost << "\\a";
    case BS: return ost << "\\b";
    case HT: return ost << "\\t";
    case LF: return ost << "\\n";
    case VT: return ost << "\\v";
    case FF: return ost << "\\f";
    case CR: return ost << "\\r";
    case ESC: return ost << "\\E";
    default: return ost << ch.c;
    }
}

#endif // COMMON__ASCII__HXX
