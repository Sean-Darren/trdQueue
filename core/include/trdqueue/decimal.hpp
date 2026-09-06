#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>

namespace trdqueue {

inline constexpr int kTickDecimals = 8;
inline constexpr std::int64_t kTickScale = 100000000LL;

inline std::optional<std::int64_t> parse_decimal_to_ticks(std::string_view s) {
    if (s.empty()) {
        return std::nullopt;
    }

    std::size_t i = 0;
    if (s[i] == '+') {
        ++i;
    }
    if (i < s.size() && s[i] == '-') {
        return std::nullopt;
    }

    const auto max64 = std::numeric_limits<std::int64_t>::max();
    std::int64_t integer = 0;
    bool saw_digit = false;

    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        saw_digit = true;
        const int d = s[i] - '0';
        if (integer > (max64 - d) / 10) {
            return std::nullopt;
        }
        integer = integer * 10 + d;
        ++i;
    }

    std::int64_t frac = 0;
    int frac_seen = 0;
    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
            saw_digit = true;
            const int d = s[i] - '0';
            if (frac_seen < kTickDecimals) {
                frac = frac * 10 + d;
                ++frac_seen;
            } else if (d != 0) {
                return std::nullopt;
            }
            ++i;
        }
    }

    if (!saw_digit || i != s.size()) {
        return std::nullopt;
    }

    if (integer > max64 / kTickScale) {
        return std::nullopt;
    }
    std::int64_t result = integer * kTickScale;
    for (int k = frac_seen; k < kTickDecimals; ++k) {
        frac *= 10;
    }
    if (result > max64 - frac) {
        return std::nullopt;
    }
    return result + frac;
}

inline std::string ticks_to_decimal(std::int64_t ticks) {
    const bool negative = ticks < 0;
    std::uint64_t mag = negative ? static_cast<std::uint64_t>(-ticks)
                                 : static_cast<std::uint64_t>(ticks);
    const std::uint64_t ip = mag / static_cast<std::uint64_t>(kTickScale);
    std::uint64_t fp = mag % static_cast<std::uint64_t>(kTickScale);

    std::string out;
    if (negative) {
        out.push_back('-');
    }
    out += std::to_string(ip);
    if (fp == 0) {
        return out;
    }

    char frac[kTickDecimals];
    for (int i = kTickDecimals - 1; i >= 0; --i) {
        frac[i] = static_cast<char>('0' + (fp % 10));
        fp /= 10;
    }
    int last = kTickDecimals - 1;
    while (last > 0 && frac[last] == '0') {
        --last;
    }
    out.push_back('.');
    out.append(frac, frac + last + 1);
    return out;
}

}
