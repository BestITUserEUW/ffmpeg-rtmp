#pragma once

#include <string_view>
#include <optional>
#include <cstdint>
#include <charconv>

// std::optional wrapper around the from chars interface

namespace st {

/**
 * @brief std::from_chars wrapper, returning nullopt if from chars reports error
 *
 * @tparam T
 * @param s
 * @return std::optional<T>
 */
template <typename T>
constexpr auto FromChars(std::string_view s) -> std::optional<T> {
    T val;
    if (std::from_chars(s.data(), s.data() + s.size(), val).ec == std::errc{}) {
        return val;
    } else {
        return std::nullopt;
    }
}

template <>
constexpr auto FromChars<bool>(std::string_view s) -> std::optional<bool> {
    return FromChars<uint8_t>(s).transform([](uint8_t val) { return static_cast<bool>(val); });
}

}  // namespace st