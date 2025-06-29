#include "rtmp_server.hpp"

#include <print>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
}

#include <oryx/enchantum.hpp>

#include "av_helpers.hpp"
#include "av_error.hpp"

namespace oryx {

RtmpServer::RtmpServer(Settings settings)
    : settings_(settings),
      frame_(av::MakeUniqueFrame()),
      dec_ctx_(),
      sws_ctx_(),
      queue_(settings.queue_size),
      on_image_(),
      on_error_(),
      on_connect_(),
      on_disconnect_(),
      read_worker_(),
      decode_worker_(),
      video_stream_index_() {}

RtmpServer::~RtmpServer() { Stop(); }

void RtmpServer::Start() {
    if (!read_worker_) {
        read_worker_ = std::make_unique<std::jthread>(&RtmpServer::ReadWorker, this);
    }
}

void RtmpServer::Stop() {
    read_worker_.reset();
    decode_worker_.reset();
}

void RtmpServer::SetImageHandler(OnImageFn on_image) { on_image_ = on_image; }
void RtmpServer::SetErrorHandler(OnErrorFn on_error) { on_error_ = on_error; }
void RtmpServer::SetConnectedHandler(OnConnectedFn on_connect) { on_connect_ = on_connect; }
void RtmpServer::SetDisconnectedHandler(OnDisconnectedFn on_disconnect) { on_disconnect_ = on_disconnect; }

void RtmpServer::SubmitError(Error&& error) const {
    if (on_error_) {
        on_error_(std::move(error));
    }
}

auto RtmpServer::Decode(AVPacket* packet) -> void_expected<av::Error> {
    auto dec = dec_ctx_.get();

    int ret = avcodec_send_packet(dec, packet);
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    // get all the available frames from the decoder
    auto frame = frame_.get();
    while (ret >= 0) {
        ret = avcodec_receive_frame(dec, frame);
        if (ret < 0) {
            // those two return values are special and mean there is no output
            // frame available, but there were no errors during decoding
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN)) {
                return kVoidExpected;
            }
            return av::UnexpectedError(ret);
        }

        sws_scale(sws_ctx_.get(), frame->data, frame->linesize, 0, frame->height, buffer_data, buffer_ls);
        cv::Mat frame_view(frame->height, frame->width, CV_8UC3, buffer_data[0], frame->width * 3);
        if (on_image_) {
            on_image_(frame_view.clone());
        }
        av_frame_unref(frame);
    }

    return kVoidExpected;
}

auto RtmpServer::OpenCodecContext() -> void_expected<av::Error> {
    int ret = av_find_best_stream(fmt_ctx_.get(), AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    video_stream_index_ = ret;
    AVStream* stream = fmt_ctx_->streams[video_stream_index_];

    const AVCodec* codec{};
    if (stream->codecpar->codec_id == AV_CODEC_ID_H264) {
        // Try nvidia hwaccel first
        // codec = avcodec_find_decoder_by_name("h264_cuvid");
    }

    if (!codec) {
        codec = avcodec_find_decoder(stream->codecpar->codec_id);
    }

    if (!codec) {
        return UnexpectedError(
            std::format("Failed to find suitable codec for id={}", static_cast<int>(stream->codecpar->codec_id)));
    }

    /* Allocate a codec context for the decoder */
    dec_ctx_ = av::MakeUniqueCodecContext(codec);

    /* Copy codec parameters from input stream to output codec context */
    ret = avcodec_parameters_to_context(dec_ctx_.get(), stream->codecpar);
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }

    /* Init the decoder */
    ret = avcodec_open2(dec_ctx_.get(), codec, NULL);
    if (ret < 0) {
        return av::UnexpectedError(ret);
    }
    return kVoidExpected;
}

void RtmpServer::DecodeWorker(std::stop_token stoken) {
    av::UniquePacketPtr packet;
    while (!stoken.stop_requested()) {
        if (!queue_.read(packet)) {
            continue;
        }

        if (!packet) {
            continue;
        }

        auto result = Decode(packet.get());
        if (!result) {
            SubmitError(std::move(result.error()));
        }
    }
}

void RtmpServer::ReadWorker(std::stop_token stoken) {
    AVDictionary* rtmp_options = nullptr;

    while (!stoken.stop_requested()) {
        av_dict_set(&rtmp_options, "listen", "1", 0);
        av_dict_set(&rtmp_options, "rtmp_buffer", std::to_string(settings_.buffer_time.count()).c_str(), 0);

        AVFormatContext* fmt_ctx = avformat_alloc_context();
        fmt_ctx->interrupt_callback.callback = [](void* stoken) -> int {
            if (!stoken) return 0;
            return reinterpret_cast<std::stop_token*>(stoken)->stop_requested();
        };
        fmt_ctx->interrupt_callback.opaque = &stoken;

        int ret = avformat_open_input(&fmt_ctx, settings_.url.c_str(), nullptr, &rtmp_options);
        if (ret < 0) {
            if (ret != AVERROR_EXIT) SubmitError(av::MakeError(ret));
            continue;
        }

        if (rtmp_options) av_dict_free(&rtmp_options);

        fmt_ctx_ = av::UniqueFormatContextPtr(fmt_ctx);

        ret = avformat_find_stream_info(fmt_ctx, NULL);
        if (ret < 0) {
            SubmitError(av::MakeError(ret));
            continue;
        }

        auto result = OpenCodecContext();
        if (!result) {
            SubmitError(std::move(result.error()));
            continue;
        }

        ImageSize dec_size(dec_ctx_->width, dec_ctx_->height);
        if (on_connect_) {
            StreamInfo info;
            info.codec = enchantum::to_string(dec_ctx_->codec->id);
            info.stream_fmt = enchantum::to_string(dec_ctx_->pix_fmt);
            info.resolution = dec_size;
            info.stream_index = video_stream_index_;
            on_connect_(info);
        }

        decode_worker_ = std::make_unique<std::jthread>(&RtmpServer::DecodeWorker, this);
        sws_ctx_ = av::GetSwsConvertFormatContext(dec_ctx_->pix_fmt, AV_PIX_FMT_BGR24, dec_size, SWS_BILINEAR);
        buffer_data_size = av_image_alloc(buffer_data, buffer_ls, dec_size.width, dec_size.height, AV_PIX_FMT_BGR24, 1);

        av_dump_format(fmt_ctx, 0, settings_.url.c_str(), 0);

        av::UniquePacketPtr packet;
        while (ret >= 0) {
            packet = av::MakeUniquePacket();
            ret = av_read_frame(fmt_ctx, packet.get());
            if (ret < 0) {
                break;
            }

            // We only want our video. Ignore everything else
            if (packet->stream_index != video_stream_index_) {
                continue;
            }

            queue_.write(std::move(packet));
        }

        if (on_disconnect_) {
            on_disconnect_();
        }

        decode_worker_.reset();
        fmt_ctx_.reset();
        sws_ctx_.reset();
        dec_ctx_.reset();
        while (!queue_.isEmpty()) queue_.popFront();
        if (buffer_data[0]) {
            av_freep(&buffer_data[0]);
        }
    }
}

}  // namespace oryx