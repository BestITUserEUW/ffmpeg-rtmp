#include <print>

#include <opencv2/opencv.hpp>

extern "C" {

#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

#include "ffmpeg_encoder.hpp"
#include "fps_limiter.hpp"

using std::println;

auto GenerateRGBFlowEffect(int frame_index, int width, int height) {
    cv::Mat yuv(height + height / 2, width, CV_8UC1);

    // Fill Y plane
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            yuv.at<uchar>(y, x) = x + y + frame_index * 3;
        }
    }

    // Fill U (Cb) plane
    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {
            yuv.at<uchar>(height + y, x * 2) = 128 + y + frame_index * 2;
        }
    }

    // Fill V (Cr) plane
    for (int y = 0; y < height / 2; y++) {
        for (int x = 0; x < width / 2; x++) {
            yuv.at<uchar>(height + y, x * 2 + 1) = 64 + x + frame_index * 5;
        }
    }

    // Convert to BGR for visualization
    cv::Mat bgr;
    cv::cvtColor(yuv, bgr, cv::COLOR_YUV420p2BGR);
    return bgr;
}

int main() {
    AVDictionary* options = NULL;
    AVFormatContext* flv_fmt_ctx = nullptr;
    AVStream* video_stream = nullptr;

    int ret;
    if ((ret = av_dict_set(&options, "listen", "2", 0)) < 0) {
        println("Failed to set listen mode for server");
        return ret;
    }

    ret = avformat_alloc_output_context2(&flv_fmt_ctx, nullptr, "flv", nullptr);
    if (ret < 0) {
        println("Could not allocate output format context!");
        return 1;
    }

    st::FFMpegEncoder::Config config;
    config.frame_width = 1280;
    config.frame_height = 720;
    config.bitrate = 4000000;
    config.frame_rate = 30;

    st::FpsLimiter limiter{config.frame_rate};
    st::FFMpegEncoder encoder{};
    auto result = encoder.Init(config);
    if (!result) {
        println("Encoder init failed with error: {}", result.error().what());
        return 1;
    }

    auto codec_ctx = encoder.codec_ctx();
    video_stream = avformat_new_stream(flv_fmt_ctx, codec_ctx->codec);
    ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0) {
        println("Could not initialize stream codec parameters!");
        return 1;
    }

    ret = avio_open2(&flv_fmt_ctx->pb, "rtmp://127.0.0.1:8080/live", AVIO_FLAG_WRITE, nullptr, &options);
    if (ret < 0) {
        println("Failed to open server");
        return 1;
    }

    av_dump_format(flv_fmt_ctx, 0, "rtmp://127.0.0.1:8080/live", 1);

    ret = avformat_write_header(flv_fmt_ctx, nullptr);
    if (ret < 0) {
        println("Could not write header!");
        exit(1);
    }

    println("Start writing frames");
    uint16_t counter{};
    const cv::Scalar text_color(0, 0, 255);
    const cv::Point point(20, 20);
    for (;;) {
        auto image = GenerateRGBFlowEffect(counter, config.frame_width, config.frame_height);
        cv::putText(image, std::format("{}", counter), point, cv::FONT_HERSHEY_SIMPLEX, 0.5, text_color, 2);
        encoder.Encode(image, [&](AVPacket* pkt) {
            println("Writing packet: {}", counter++);
            av_packet_rescale_ts(pkt, encoder.codec_ctx()->time_base, video_stream->time_base);
            pkt->stream_index = video_stream->index;

            av_interleaved_write_frame(flv_fmt_ctx, pkt);
        });
        limiter.Sleep();
    }

    avio_close(flv_fmt_ctx->pb);
    avformat_free_context(flv_fmt_ctx);

    return 0;
}