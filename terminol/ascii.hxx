// vi:noai:sw=4

#ifndef ASCII__HXX
#define ASCII__HXX

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
const char ESC   = '\x1B';
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

#endif // ASCII__HXX
