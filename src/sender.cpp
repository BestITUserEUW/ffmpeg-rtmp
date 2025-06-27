#include <print>
#include <generator>
#include <functional>
#include <format>
#include <csignal>

#include <opencv2/opencv.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
}

#include "h264_encoder.hpp"
#include "fps_limiter.hpp"

using std::println;
using namespace st;

using RgbFlowEffectGenerator = std::generator<std::pair<Image, int>>;
using GenStopFn = std::function<bool()>;

static std::atomic_bool should_exit{};

static auto CreateRgbFlowEffectGenerator(int width, int height) -> RgbFlowEffectGenerator {
    Image yuv(height + height / 2, width, CV_8UC1);
    int index{};

    while (!should_exit.load(std::memory_order_relaxed)) {
        // Fill Y plane
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                yuv.at<uchar>(y, x) = x + y + index * 3;
            }
        }

        // Fill U (Cb) plane
        for (int y = 0; y < height / 2; y++) {
            for (int x = 0; x < width / 2; x++) {
                yuv.at<uchar>(height + y, x * 2) = 128 + y + index * 2;
            }
        }

        // Fill V (Cr) plane
        for (int y = 0; y < height / 2; y++) {
            for (int x = 0; x < width / 2; x++) {
                yuv.at<uchar>(height + y, x * 2 + 1) = 64 + x + index * 5;
            }
        }

        Image bgr;
        cv::cvtColor(yuv, bgr, cv::COLOR_YUV420p2BGR);
        index++;
        co_yield std::make_pair(bgr, index);
    }
}

auto main(int argc, char* argv[]) -> int {
    if (argc < 2) {
        println("Example Usage:\n {} rtmp://127.0.0.1:8080/live", argv[0]);
        return 1;
    }

    const char* url = argv[1];

    signal(SIGINT, [](int) {
        println("\nUser Interrupt");
        should_exit.store(true, std::memory_order_relaxed);
    });

    H264Encoder::Settings settings;
    settings.frame_width = 1920;
    settings.frame_height = 1080;
    settings.bitrate = 4000000;
    settings.frame_rate = 60;

    FpsLimiter limiter{settings.frame_rate};
    H264Encoder encoder{};
    auto result = encoder.Init(settings);
    if (!result) {
        println("Failed to initialize encoder with error={}", result.error().what());
        return 1;
    }

    libav::UniqueFormatContextPtr fmt_ctx([] {
        AVFormatContext* raw{};
        avformat_alloc_output_context2(&raw, nullptr, "flv", nullptr);
        return raw;
    }());

    if (!fmt_ctx) {
        println("Failed to alloc format context");
        return 1;
    }

    auto codec_ctx = encoder.codec_ctx();
    auto video_stream = avformat_new_stream(fmt_ctx.get(), codec_ctx->codec);
    int ret = avcodec_parameters_from_context(video_stream->codecpar, codec_ctx);
    if (ret < 0) {
        println("Could not initialize stream codec params with error={}", av_err2str(ret));
        return 1;
    }

    println("Trying to connect to {}", url);
    libav::UniqueIoContextPtr rtmp_ctx{[&] {
        AVIOContext* raw{};
        AVDictionary* opts{};

        av_dict_set(&opts, "rtmp_live", "live", 0);
        avio_open2(&raw, url, AVIO_FLAG_WRITE, nullptr, &opts);
        av_dict_free(&opts);
        return raw;
    }()};
    if (!rtmp_ctx) {
        println("Failed to establish connection", url);
        return 1;
    }
    fmt_ctx->pb = rtmp_ctx.get();

    av_dump_format(fmt_ctx.get(), 0, url, 1);

    ret = avformat_write_header(fmt_ctx.get(), nullptr);
    if (ret < 0) {
        println("Could not write header!");
        return 1;
    }

    const cv::Scalar kTextColor(0, 0, 255);
    const cv::Point kTextPoint(20, 20);
    println("Start sending frames");
    for (auto [image, index] : CreateRgbFlowEffectGenerator(settings.frame_width, settings.frame_height)) {
        cv::putText(image, std::format("{}", index), kTextPoint, cv::FONT_HERSHEY_SIMPLEX, 0.5, kTextColor, 2);
        encoder.Encode(image, [&](AVPacket* pkt) {
            av_packet_rescale_ts(pkt, codec_ctx->time_base, video_stream->time_base);
            pkt->stream_index = video_stream->index;
            std::print("Sending packet={}\r", index);
            fflush(stdout);
            ret = av_interleaved_write_frame(fmt_ctx.get(), pkt);
            if (ret < 0) println("Failed to write packet!");
        });
        limiter.Sleep();
    }

    println("Exiting");
    return 0;
}