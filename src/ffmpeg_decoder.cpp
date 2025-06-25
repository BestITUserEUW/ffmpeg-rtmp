#include "ffmpeg_decoder.hpp"

#include <print>
#include <opencv2/core/mat.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include "ffmpeg_helpers.hpp"

using std::println;

namespace st {

FFMpegDecoder::~FFMpegDecoder() { DeInit(); }

auto FFMpegDecoder::Init() -> void_expected {
    auto codec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec) {
        return UnexpectedError("h264 codec not found");
    }

    parser_ = MakeAvCodecParserContextPtr(codec->id);
    if (!parser_) {
        return UnexpectedError("parser not found");
    }

    codec_ctx_ = MakeAvCodecContextPtr(codec);
    if (!codec_ctx_) {
        return UnexpectedError("alloc video ctx failed");
    }

    codec_ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    codec_ctx_->flags2 |= AV_CODEC_FLAG2_SHOW_ALL;

    av_opt_set(codec_ctx_->priv_data, "preset", "medium", 0);
    av_opt_set(codec_ctx_->priv_data, "tune", "zerolatency", 0);

    if (avcodec_open2(codec_ctx_.get(), codec, nullptr) < 0) {
        return UnexpectedError("open codec failed");
    }

    yuv_frame_ = MakeAvFramePtr();
    if (!yuv_frame_) {
        return UnexpectedError("yuv frame alloc failed");
    }

    parsed_pkt_ = MakeAvPacketPtr();
    if (!parsed_pkt_) {
        return UnexpectedError("h264 pkt alloc failed");
    }

    return void_ok;
}

void FFMpegDecoder::DeInit() {
    yuv_frame_.reset();
    sws_ctx_.reset();
    parser_.reset();
    codec_ctx_.reset();
}

auto FFMpegDecoder::Decode(const H264Image &encoded, Image &decoded) -> FFMpegDecoder::ErrorKind {
    assert(parser_ != nullptr && "Codec parser is not initialized");
    assert(codec_ctx_ != nullptr && "Codec ctx is not initialized");

    auto data_it = encoded.data();
    int data_size = encoded.size();
    int processed_size;
    while (data_size > 0) {
        processed_size = av_parser_parse2(parser_.get(), codec_ctx_.get(), &parsed_pkt_->data, &parsed_pkt_->size,
                                          data_it, data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (processed_size < 0) {
            return ErrorKind::kParseFailed;
        }

        data_size -= processed_size;
        data_it += processed_size;
        if (!parsed_pkt_->size) {
            continue;
        }

        int ret = avcodec_send_packet(codec_ctx_.get(), parsed_pkt_.get());
        if (ret < 0) {
            return ErrorKind::kSendFailed;
        }

        while (ret >= 0) {
            ret = avcodec_receive_frame(codec_ctx_.get(), yuv_frame_.get());
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                continue;
            } else if (ret < 0) {
                return ErrorKind::kDecodeFailed;
            }

            if (yuv_frame_->width != decode_width_ || yuv_frame_->height != decode_height_) {
                UpdateInputSize();
            }

            if (sws_ctx_ && buffer_[0]) {
                sws_scale(sws_ctx_.get(), yuv_frame_->data, yuv_frame_->linesize, 0, yuv_frame_->height, buffer_,
                          buffer_ls_);
                cv::Mat view(decode_height_, decode_width_, CV_8UC3, buffer_[0], decode_width_ * 3);
                decoded = view.clone();
                return ErrorKind::kOk;
            }
        }
        av_packet_unref(parsed_pkt_.get());
        // We don't have to unref frame here as avcodec_receive_frame does that before using frame.
    }
    return ErrorKind::kNoFrame;
}

void FFMpegDecoder::UpdateInputSize() {
    println("Setting new input size w: {} h: {}", yuv_frame_->width, yuv_frame_->height);
    decode_height_ = yuv_frame_->height;
    decode_width_ = yuv_frame_->width;

    sws_ctx_.reset(sws_getContext(decode_width_, decode_height_, codec_ctx_->pix_fmt, decode_width_, decode_height_,
                                  AV_PIX_FMT_BGR24, SWS_BILINEAR, nullptr, nullptr, nullptr));
    if (buffer_[0]) {
        av_freep(&buffer_[0]);
    }
    av_image_alloc(buffer_, buffer_ls_, decode_width_, decode_height_, AV_PIX_FMT_BGR24, 1);
}

}  // namespace st
