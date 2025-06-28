#pragma once

#include <cstddef>
#include <functional>

#include "expected.hpp"
#include "image.hpp"
#include "libav_helpers.hpp"

namespace st {

class H264Encoder {
public:
    using OnPacketFn = std::function<void(libav::UniquePacketPtr)>;

    struct Settings {
        ImageSize size;
        int frame_rate;
        int bitrate;
    };

    H264Encoder();
    ~H264Encoder();

    auto Open(Settings settings) -> void_expected;
    void Close();

    auto Encode(const Image& image, OnPacketFn on_packet) -> void_expected;
    auto codec_ctx() { return codec_ctx_.get(); }

private:
    libav::UniqueCodecContextPtr codec_ctx_;
    libav::UniqueSwsContextPtr sws_ctx_;
    libav::UniqueFramePtr frame_;
};

}  // namespace st