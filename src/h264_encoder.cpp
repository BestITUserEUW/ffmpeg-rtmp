#include "h264_encoder.hpp"

#include <cassert>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

[[maybe_unused]] static constexpr char* kMissingInitMessage = "Init should be called before using H264Encoder::Encode";

namespace st {

H264Encoder::~H264Encoder() { DeInit(); }

auto H264Encoder::Init(Settings settings) -> void_expected {
    auto codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        // Let ffmpeg decide if nvenc is not available
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec) {
            return UnexpectedError("No h264 encoder found");
        }
    }

    codec_ctx_ = libav::MakeUniqueCodecContext(codec);
    codec_ctx_->bit_rate = settings.bitrate;
    codec_ctx_->width = settings.frame_width;
    codec_ctx_->height = settings.frame_height;
    codec_ctx_->time_base = AVRational{1, settings.frame_rate};
    codec_ctx_->framerate = AVRational{settings.frame_rate, 1};
    codec_ctx_->gop_size = 5;
    codec_ctx_->max_b_frames = 1;
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    int rc = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (rc < 0) {
        return UnexpectedError("open codec failed");
    }

    packet_ = libav::MakeUniquePacket();
    if (!packet_) {
        return UnexpectedError("alloc pkt failed");
    }

    frame_ = libav::MakeUniqueFrame();
    if (!frame_) {
        return UnexpectedError("alloc frame failed");
    }

    frame_->format = codec_ctx_->pix_fmt;
    frame_->width = settings.frame_width;
    frame_->height = settings.frame_height;
    frame_->pts = 0;

    rc = av_frame_get_buffer(frame_.get(), 0);
    if (rc < 0) {
        return UnexpectedError("frame get buffer failed");
    }

    sws_ctx_ = libav::GetSwsConvertFormatContext(AV_PIX_FMT_BGR24, codec_ctx_->pix_fmt, settings.frame_width,
                                                 settings.frame_height, SWS_BILINEAR);
    if (!sws_ctx_) {
        return UnexpectedError("Failed to get sws context");
    }
    return void_ok;
}

void H264Encoder::DeInit() {
    sws_ctx_.reset();
    codec_ctx_.reset();
    packet_.reset();
    frame_.reset();
}

auto H264Encoder::Encode(const Image& image, OnDataFn on_data) -> H264Encoder::Error {
    assert(packet_ && kMissingInitMessage);
    assert(frame_ && kMissingInitMessage);
    assert(codec_ctx_ && kMissingInitMessage);
    assert(sws_ctx_ && kMissingInitMessage);

    frame_->pts++;

    int ret = av_frame_make_writable(frame_.get());
    if (ret < 0) {
        return Error::kUnknown;
    }

    const uint8_t* frame_slice[] = {image.data};
    int frame_stride[] = {static_cast<int>(image.step)};
    auto codec_ctx_ptr = codec_ctx_.get();
    sws_scale(sws_ctx_.get(), frame_slice, frame_stride, 0, codec_ctx_ptr->height, frame_->data, frame_->linesize);

    ret = avcodec_send_frame(codec_ctx_ptr, frame_.get());
    if (ret < 0) {
        return Error::kSendFailed;
    }

    auto packet_ptr = packet_.get();

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_ptr, packet_ptr);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            continue;
        else if (ret < 0) {
            return Error::kEncodeFailed;
        }
        on_data(packet_ptr);
        av_packet_unref(packet_ptr);
        return Error::kOk;
    }
    return Error::kNoFrame;
}

}  // namespace st