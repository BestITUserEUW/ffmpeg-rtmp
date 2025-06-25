#pragma once

#include <string>
#include <string_view>
#include <expected>

#include "error.hpp"

namespace st {

template <typename T>
using expected = std::expected<T, Error>;
using unexpected = std::unexpected<Error>;
using void_expected = expected<void>;

inline constexpr void_expected void_ok{};

[[nodiscard]] inline auto UnexpectedError(std::string&& what) { return unexpected(std::forward<std::string>(what)); }
[[nodiscard]] inline auto UnexpectedError(const char* what) { return unexpected(what); }
[[nodiscard]] inline auto UnexpectedError(const std::string& what) { return unexpected(what); }
[[nodiscard]] inline auto UnexpectedError(const std::exception& exc) { return unexpected(exc.what()); }
}  // namespace st