// vi:noai:sw=4

#ifndef ASCII__HXX
#define ASCII__HXX

#include <iostream>

const char NUL   = '\x00';  // '\0'
const char SOH   = '\x01';
const char STX   = '\x02';
const char ETX   = '\x03';
const char EOT   = '\x04';
const char ENQ   = '\x05';
const char ACK   = '\x06';
const char BEL   = '\x07';  // '\a'
const char BS    = '\x08';  // '\b'
const char HT    = '\x09';  // '\t'
const char LF    = '\x0A';  // '\n'
const char VT    = '\x0B';  // '\v'
const char FF    = '\x0C';  // '\f'
const char CR    = '\x0D';  // '\r'
const char SO    = '\x0E';
const char SI    = '\x0F';
const char DLE   = '\x10';
const char DC1   = '\x11';
const char DC2   = '\x12';
const char DC3   = '\x13';
const char DC4   = '\x14';
const char NAK   = '\x15';
const char SYN   = '\x16';
const char ETB   = '\x17';
const char CAN   = '\x18';
const char EM    = '\x19';
const char SUB   = '\x1A';
const char ESC   = '\x1B';  // '\E'
const char FS    = '\x1C';
const char GS    = '\x1D';
const char RS    = '\x1E';
const char US    = '\x1F';
const char SPACE = '\x20';

// '\x21'..'\x40': '!'..'@'
// '\x41'..'\x5A': 'A'..'Z'
//
// '\x61'..'\x7A': 'a'..'z'

const char DEL   = '\x7F';

inline bool isControl(char c) {     // XXX remove me
    return (c >= NUL && c < SPACE) || c == DEL;
}

// Streaming helper.
struct Char {
    explicit Char(char c_) : c(c_) {}
    char c;
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

struct Str {
    std::string s;
    explicit Str(std::string s_) : s(s_) {}     // The C++11 way, right?
};

inline std::ostream & operator << (std::ostream & ost, const Str & str) {
    for (auto c : str.s) {
        ost << Char(c);
    }

    return ost;
}

#endif // ASCII__HXX
