//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#ifndef RME_UTIL_NUMBER_INPUT_H_
#define RME_UTIL_NUMBER_INPUT_H_

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <string_view>
#include <system_error>

// Resolves the text of a numeric input field. A whole integer, optionally surrounded by
// whitespace, is clamped to [minval, maxval]. Anything else keeps the previous value:
// empty text, a value too large for int64_t, and text with other characters. Digits are
// never pulled out of other text, so a pasted "512, 1024, 7" cannot become 51210247.
[[nodiscard]] inline int64_t ResolveNumberInput(std::string_view text, int64_t minval, int64_t maxval, int64_t lastval) {
    if (minval > maxval) {
        return minval;
    }
    const int64_t fallback = std::clamp(lastval, minval, maxval);

    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.front()))) {
        text.remove_prefix(1);
    }
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back()))) {
        text.remove_suffix(1);
    }
    if (text.empty()) {
        return fallback;
    }

    int64_t value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc() || end != text.data() + text.size()) {
        return fallback;
    }
    return std::clamp(value, minval, maxval);
}

#endif
