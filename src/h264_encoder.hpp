#pragma once

#include <cstddef>
#include <functional>

#include "expected.hpp"
#include "image.hpp"
#include "libav_helpers.hpp"

namespace st {

class H264Encoder {
public:
    enum class Error { kOk, kSendFailed, kEncodeFailed, kParseFailed, kNoFrame, kUnknown };
    using OnDataFn = std::function<void(AVPacket*)>;

    struct Settings {
        int frame_width;
        int frame_height;
        int frame_rate;
        int bitrate;
    };

    H264Encoder() = default;
    ~H264Encoder();

    auto Init(Settings settings) -> void_expected;
    void DeInit();

    auto Encode(const Image& image, OnDataFn on_data) -> Error;
    auto codec_ctx() { return codec_ctx_.get(); }

private:
    libav::UniqueCodecContextPtr codec_ctx_{};
    libav::UniqueSwsContextPtr sws_ctx_{};
    libav::UniqueFramePtr frame_{};
    libav::UniquePacketPtr packet_{};
};

}  // namespace st