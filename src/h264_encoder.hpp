#pragma once

#include <cstddef>
#include <functional>

#include "image.hpp"
#include "av_helpers.hpp"
#include "av_error.hpp"

namespace oryx {

class H264Encoder {
public:
    using OnPacketFn = std::function<void(av::UniquePacketPtr)>;

    struct Settings {
        ImageSize size;
        int frame_rate;
        int bitrate;
    };

    H264Encoder();
    ~H264Encoder();

    auto Open(Settings settings) -> void_expected<av::Error>;
    void Close();

    auto Encode(const Image& image, OnPacketFn on_packet) -> void_expected<av::Error>;
    auto codec_ctx() { return codec_ctx_.get(); }

private:
    av::UniqueCodecContextPtr codec_ctx_;
    av::UniqueSwsContextPtr sws_ctx_;
    av::UniqueFramePtr frame_;
};

}  // namespace oryx