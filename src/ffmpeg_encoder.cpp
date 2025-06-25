#include "ffmpeg_encoder.hpp"

#include <algorithm>
#include <ranges>
#include <format>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

namespace st {

FFMpegEncoder::~FFMpegEncoder() { DeInit(); }

auto FFMpegEncoder::Init(Config config) -> void_expected {
    auto codec = avcodec_find_encoder_by_name("libx264");
    if (!codec) {
        return UnexpectedError("h264 codec not found");
    }

    codec_ctx_ = MakeAvCodecContextPtr(codec);
    codec_ctx_->bit_rate = config.bitrate;
    codec_ctx_->width = config.frame_width;
    codec_ctx_->height = config.frame_height;
    codec_ctx_->time_base = AVRational{1, config.frame_rate};
    codec_ctx_->framerate = AVRational{config.frame_rate, 1};
    codec_ctx_->gop_size = 3;
    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    av_opt_set(codec_ctx_->priv_data, "preset", "medium", 0);
    av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);
    int rc = avcodec_open2(codec_ctx_.get(), codec, nullptr);
    if (rc < 0) {
        return UnexpectedError("open codec failed");
    }

    encoded_pkt_ = MakeAvPacketPtr();
    if (!encoded_pkt_) {
        return UnexpectedError("alloc pkt failed");
    }

    frame_yuv_ = MakeAvFramePtr();
    if (!frame_yuv_) {
        return UnexpectedError("alloc frame failed");
    }

    frame_yuv_->format = codec_ctx_->pix_fmt;
    frame_yuv_->width = config.frame_width;
    frame_yuv_->height = config.frame_height;
    frame_yuv_->pts = 0;

    rc = av_frame_get_buffer(frame_yuv_.get(), 0);
    if (rc < 0) {
        return UnexpectedError("frame get buffer failed");
    }

    sws_ctx_ = SwsContextPtr(sws_getContext(config.frame_width, config.frame_height, AV_PIX_FMT_BGR24,
                                            config.frame_width, config.frame_height, codec_ctx_->pix_fmt, SWS_BILINEAR,
                                            nullptr, nullptr, nullptr));
    if (!sws_ctx_) {
        return UnexpectedError("Failed to get sws context");
    }
    return void_ok;
}

void FFMpegEncoder::DeInit() {
    sws_ctx_.reset();
    codec_ctx_.reset();
    encoded_pkt_.reset();
    frame_yuv_.reset();
}

auto FFMpegEncoder::Encode(const Image& decoded, OnDataFn on_data) -> FFMpegEncoder::ErrorKind {
    frame_yuv_->pts++;

    int ret = av_frame_make_writable(frame_yuv_.get());
    if (ret < 0) {
        return ErrorKind::kUnknown;
    }

    const uint8_t* frame_slice[] = {decoded.data};
    int frame_stride[] = {static_cast<int>(decoded.step)};
    sws_scale(sws_ctx_.get(), frame_slice, frame_stride, 0, codec_ctx_->height, frame_yuv_->data, frame_yuv_->linesize);

    ret = avcodec_send_frame(codec_ctx_.get(), frame_yuv_.get());
    if (ret < 0) {
        return ErrorKind::kSendFailed;
    }

    auto encoded_pkt = encoded_pkt_.get();

    while (ret >= 0) {
        ret = avcodec_receive_packet(codec_ctx_.get(), encoded_pkt);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            continue;
        else if (ret < 0) {
            return ErrorKind::kEncodeFailed;
        }
        on_data(encoded_pkt);
        av_packet_unref(encoded_pkt);
        return ErrorKind::kOk;
    }
    return ErrorKind::kNoFrame;
}

}  // namespace st