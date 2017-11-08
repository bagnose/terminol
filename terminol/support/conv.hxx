// vi:noai:sw=4
// Copyright © 2013 David Bryant

#ifndef SUPPORT__CONV__HXX
#define SUPPORT__CONV__HXX

#include "terminol/support/debug.hxx"
#include "terminol/support/exception.hxx"

#include <string>
#include <vector>
#include <sstream>
#include <cmath>
#include <optional>

template <typename T>
T clamp(T val, T min, T max) {
    ASSERT(min <= max, );
    if      (val < min) { return min; }
    else if (val > max) { return max; }
    else                { return val; }
}

std::optional<std::vector<std::string>> split(const std::string & line,
                                              const std::string & delim = "\t ");

namespace detail {

template <typename T>
void to_ostream(std::ostream & ost, const T & v) {
    ost << v;
}

inline void to_ostream(std::ostream & ost, const bool & v) {
    std::ios_base::fmtflags flags = ost.flags(); // stash current format flags
    ost << std::boolalpha << v;
    ost.flags(flags); // reset to previous format flags
}

template <typename T, typename... Args>
void to_ostream(std::ostream & ost, const T & first, const Args &... remaining) {
    to_ostream(ost, first);
    to_ostream(ost, remaining...);
}

} // namespace detail

template <typename T, typename... Args>
std::string stringify(const T & first, const Args &... remaining) {
    std::ostringstream ost;
    detail::to_ostream(ost, first, remaining...);
    return ost.str();
}

template <typename T>
T unstringify(const std::string & str) {
    std::istringstream ist(str + '\n');
    auto t = T{};
    ist >> t;
    THROW_UNLESS(ist.good(), ConversionError("Failed to unstringify: '" + str + "'"));
    return t;
}

template <>
inline std::string unstringify<>(const std::string & str) {
    return str;
}

template <>
inline bool unstringify<>(const std::string & str) {
    if (str == "0" || str == "false") {
        return false;
    }
    else if (str == "1" || str == "true") {
        return true;
    }
    else { THROW(ConversionError("Failed to unstringify: " + str)); }
}

template <typename T>
std::string explicitSign(T t) {
    static_assert(std::is_arithmetic_v<T>);
    std::ostringstream ost;
    if (t < 0) { ost << '-'; }
    else       { ost << '+'; }
    ost << std::abs(t);
    return ost.str();
}

template <typename T>
std::string nthStr(T t) {
    static_assert(std::is_integral_v<T>);
    std::ostringstream ost;
    ost << t;
    switch (t) {
        case 0:
            ost << "th";
            break;
        case 1:
            ost << "st";
            break;
        case 2:
            ost << "nd";
            break;
        case 3:
            ost << "rd";
            break;
        default:
            ost << "th";
            break;
    }
    return ost.str();
}

inline char nibbleToHex(uint8_t nibble) {
    ASSERT(nibble < 0x10, );
    if (nibble < 0xA) { return '0' +  nibble;       }
    else              { return 'A' + (nibble - 10); }
}

inline uint8_t hexToNibble(char hex) {
    if      (hex >= '0' && hex <= '9') { return hex - '0'; }
    else if (hex >= 'A' && hex <= 'F') { return 10 + hex - 'A'; }
    else if (hex >= 'a' && hex <= 'f') { return 10 + hex - 'a'; }
    else {
        std::ostringstream ost;
        ost << "Illegal hex char: " << hex;
        THROW(ConversionError(ost.str()));
    }
}

inline void byteToHex(uint8_t byte, char & hex0, char & hex1) {
    hex0 = nibbleToHex((byte >> 4) & 0x0F);
    hex1 = nibbleToHex(byte & 0x0F);
}

inline uint8_t hexToByte(char hex0, char hex1) {
    return (hexToNibble(hex0) << 4) + hexToNibble(hex1);
}

template <typename T>
std::string toHexString(T t) {
    std::string str;
    for (size_t i = 0; i < sizeof(T); ++i) {
        str.push_back(nibbleToHex(t >> (8 * (sizeof(T) - i - 1) + 4) & 0x0F));
        str.push_back(nibbleToHex(t >> (8 * (sizeof(T) - i - 1))     & 0x0F));
    }
    return str;
}

template <typename T>
std::string toBinaryString(T t) {
    std::string str;
    for (size_t i = 0; i < 8 * sizeof(T); ++i) {
        str.push_back(((t >> (8 * sizeof(T) - i - 1)) & 0x1) ? '1' : '0');
    }
    return str;
}

inline bool exor(bool a, bool b) noexcept {
    return a != b;
}

inline std::string humanSize(size_t bytes) {
    const char * UNITS[] = {
        "B",
        "KB",
        "MB",
        "GB",
        "PB",
        "EB",
        "ZB"
    };

    auto offset = 0;
    auto value  = bytes;

    while (value >= 1024) {
        value /= 1024;
        ++offset;
    }

    std::ostringstream ost;
    ost << value << UNITS[offset];
    return ost.str();
}

#endif // SUPPORT__CONV__HXX
