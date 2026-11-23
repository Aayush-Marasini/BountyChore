// Common.h - shared helpers: checked integer math, number parsing/formatting.
// All point values are stored internally as int64 "half-point" units
// (1 point = 2 units) so urgency multipliers of 1.5 / 2.5 stay exact.
#pragma once
#include <cctype>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace bc {

using int64 = std::int64_t;

// Both helpers throw std::overflow_error instead of silently wrapping.
inline int64 checkedAdd(int64 a, int64 b) {
    if ((b > 0 && a > std::numeric_limits<int64>::max() - b) ||
        (b < 0 && a < std::numeric_limits<int64>::min() - b))
        throw std::overflow_error("arithmetic overflow");
    return a + b;
}
// Only used with non-negative operands in this program.
inline int64 checkedMul(int64 a, int64 b) {
    if (a == 0 || b == 0) return 0;
    if (a < 0 || b < 0) throw std::overflow_error("negative operand");
    if (a > std::numeric_limits<int64>::max() / b)
        throw std::overflow_error("arithmetic overflow");
    return a * b;
}

inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// Parses a non-negative whole number made only of digits (max 18 digits).
inline bool parseWhole(const std::string& s, int64& out) {
    if (s.empty() || s.size() > 18) return false;
    int64 v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
    }
    out = v;
    return true;
}

// Parses "12", "12.0" or "12.5" into half-point units (12 -> 24, 12.5 -> 25).
inline bool parseHalf(const std::string& s, int64& out) {
    size_t dot = s.find('.');
    std::string whole = (dot == std::string::npos) ? s : s.substr(0, dot);
    std::string frac = (dot == std::string::npos) ? "" : s.substr(dot + 1);
    if (whole.empty() || whole.size() > 15) return false;
    int64 w = 0;
    if (!parseWhole(whole, w)) return false;
    int64 half = 0;
    if (dot != std::string::npos) {
        if (frac.empty()) return false;
        if (frac[0] == '5') half = 1;
        else if (frac[0] != '0') return false;
        for (size_t i = 1; i < frac.size(); ++i)
            if (frac[i] != '0') return false;
    }
    out = w * 2 + half;
    return true;
}

// Half-point units -> "12.5" / "12.0".
inline std::string formatHalf(int64 h) {
    return std::to_string(h / 2) + (h % 2 ? ".5" : ".0");
}

// IDs: 1-32 chars of letters, digits, '-' or '_' (keeps CSV and lookups simple).
inline bool validId(const std::string& s) {
    if (s.empty() || s.size() > 32) return false;
    for (unsigned char c : s)
        if (!(std::isalnum(c) || c == '-' || c == '_')) return false;
    return true;
}
// Names / rooms: 1-60 printable characters.
inline bool validText(const std::string& s) {
    if (s.empty() || s.size() > 60) return false;
    for (unsigned char c : s)
        if (c < 32 || c == 127) return false;
    return true;
}

} // namespace bc
