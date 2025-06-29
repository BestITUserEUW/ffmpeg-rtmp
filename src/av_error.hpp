#pragma once

#include <expected>

extern "C" {
#include <libavutil/error.h>
}

#include <oryx/expected.hpp>

namespace oryx::av {

class Error : public oryx::Error {
public:
    Error(std::string what, int error_code)
        : oryx::Error(std::move(what)),
          error_code_(error_code) {}

    Error(const oryx::Error& error)
        : oryx::Error(error),
          error_code_(AVERROR_EXTERNAL) {}

    auto error_code() const -> int { return error_code_; }

private:
    int error_code_;
};

inline auto MakeError(int error_code) { return Error(av_err2str(error_code), error_code); }
inline auto UnexpectedError(int error_code) { return std::unexpected<Error>(MakeError(error_code)); }

}  // namespace oryx::av