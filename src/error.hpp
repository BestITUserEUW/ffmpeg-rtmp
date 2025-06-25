#pragma once

#include <string>

namespace st {
class Error {
public:
    explicit Error(const std::string& _what)
        : what_(_what) {}

    explicit Error(std::string&& _what)
        : what_(std::move(_what)) {}

    ~Error() = default;

    const std::string& what() const { return what_; }

private:
    std::string what_;
};
};  // namespace st