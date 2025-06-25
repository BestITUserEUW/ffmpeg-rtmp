#pragma once

#include <cstddef>
#include <vector>
#include <functional>

#include <opencv2/core/mat.hpp>

#include "expected.hpp"
#include "image.hpp"
#include "ffmpeg_helpers.hpp"

namespace st {

class FFMpegEncoder {
public:
    enum class ErrorKind { kOk, kSendFailed, kEncodeFailed, kParseFailed, kNoFrame, kUnknown };
    using OnDataFn = std::function<void(AVPacket*)>;

    struct Config {
        int frame_width;
        int frame_height;
        int frame_rate;
        int bitrate;
    };

    FFMpegEncoder() = default;
    ~FFMpegEncoder();

    auto Init(Config config) -> void_expected;
    void DeInit();

    auto Encode(const Image& decoded, OnDataFn on_data) -> ErrorKind;
    auto codec_ctx() { return codec_ctx_.get(); }

private:
    AvCodecContextPtr codec_ctx_{};
    SwsContextPtr sws_ctx_{};
    AvFramePtr frame_yuv_{};
    AvPacketPtr encoded_pkt_{};
};

}  // namespace st