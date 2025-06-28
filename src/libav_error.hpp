#pragma once

#include <expected>

extern "C" {
#include <libavutil/error.h>
}

#include "error.hpp"

namespace st::libav {

inline auto MakeError(int error) { return Error(av_err2str(error)); }
inline auto UnexpectedError(int error) { return std::unexpected<Error>(MakeError(error)); }

}  // namespace st::libav