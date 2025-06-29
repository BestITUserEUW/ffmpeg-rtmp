#include "h264_encoder.hpp"

#include <cassert>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "av_error.hpp"

[[maybe_unused]] static constexpr char kMissingInitMessage[] = "Init should be called before using H264Encoder::Encode";

namespace oryx {

H264Encoder::H264Encoder()
    : codec_ctx_(),
      sws_ctx_(),
      frame_(av::MakeUniqueFrame()) {}

H264Encoder::~H264Encoder() { Close(); }

auto H264Encoder::Open(Settings settings) -> void_expected<av::Error> {
    auto codec = avcodec_find_encoder_by_name("h264_nvenc");
    if (!codec) {
        // Let ffmpeg decide if nvenc is not available
        codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }

    if (!codec) {
        return av::UnexpectedError(AVERROR_ENCODER_NOT_FOUND);
    }

    codec_ctx_ = av::MakeUniqueCodecContext(codec);
    codec_ctx_->bit_rate = settings.bitrate;
    codec_ctx_->width = settings.size.width;
    codec_ctx_->height = settings.size.height;
    codec_ctx_->time_base = AVRational{1, settings.frame_rate};
    codec_ctx_->framerate = AVRational{settings.frame_rate, 1};
    codec_ctx_->gop_size = 5;
    codec_ctx_->max_b_frames = 0;
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    int ret = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    frame_->format = codec_ctx_->pix_fmt;
    frame_->width = settings.size.width;
    frame_->height = settings.size.height;
    frame_->pts = 0;

    ret = av_frame_get_buffer(frame_.get(), 0);
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    sws_ctx_ = av::GetSwsConvertFormatContext(AV_PIX_FMT_BGR24, codec_ctx_->pix_fmt, settings.size, SWS_BILINEAR);
    if (!sws_ctx_) {
        return av::UnexpectedError(ret);
    }
    return kVoidExpected;
}

void H264Encoder::Close() {
    sws_ctx_.reset();
    codec_ctx_.reset();
}

auto H264Encoder::Encode(const Image& image, OnPacketFn on_packet) -> void_expected<av::Error> {
    assert(frame_ && kMissingInitMessage);
    assert(codec_ctx_ && kMissingInitMessage);
    assert(sws_ctx_ && kMissingInitMessage);

    if (!on_packet) {
        return UnexpectedError("Packet callback is invalid");
    }

    int ret = av_frame_make_writable(frame_.get());
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    const uint8_t* frame_slice[] = {image.data};
    int frame_stride[] = {static_cast<int>(image.step)};
    auto codec_ctx_ptr = codec_ctx_.get();
    sws_scale(sws_ctx_.get(), frame_slice, frame_stride, 0, codec_ctx_ptr->height, frame_->data, frame_->linesize);

    ret = avcodec_send_frame(codec_ctx_ptr, frame_.get());
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    frame_->pts++;

    while (ret >= 0) {
        auto packet = av::MakeUniquePacket();
        ret = avcodec_receive_packet(codec_ctx_ptr, packet.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return kVoidExpected;
        } else if (ret < 0) {
            return av::UnexpectedError(ret);
        } else {
            on_packet(std::move(packet));
        }
    }
    return kVoidExpected;
}

}  // namespace oryx