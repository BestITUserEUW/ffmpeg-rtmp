#pragma once

#include <cstdint>
#include <vector>

#include <opencv2/core/mat.hpp>

#include "expected.hpp"
#include "image.hpp"
#include "ffmpeg_helpers.hpp"

namespace st {

class FFMpegDecoder {
public:
    enum class ErrorKind { kOk, kSendFailed, kDecodeFailed, kParseFailed, kNoFrame };

    using Image = cv::Mat;

    FFMpegDecoder() = default;
    ~FFMpegDecoder();

    auto Init() -> void_expected;
    void DeInit();
    auto Decode(const H264Image &encoded, Image &decoded) -> ErrorKind;

private:
    void UpdateInputSize();

    AvCodecContextPtr codec_ctx_{};
    AvCodecParserContextPtr parser_{};
    SwsContextPtr sws_ctx_{};
    AvFramePtr yuv_frame_{};
    AvPacketPtr parsed_pkt_{};
    int decode_height_{};
    int decode_width_{};
    uint8_t *buffer_[4]{};
    int buffer_ls_[4]{};
};

}  // namespace st
