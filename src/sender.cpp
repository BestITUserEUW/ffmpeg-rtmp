#include <print>
#include <generator>
#include <functional>
#include <format>
#include <optional>
#include <csignal>
#include <atomic>

#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/opt.h>
}

#include <oryx/chrono/frame_rate_controller.hpp>
#include <oryx/argparse.hpp>

#include "h264_encoder.hpp"

using std::println;
using namespace oryx;

using ImageGenerator = std::generator<Image>;

static auto CreateRgbFlowEffectGenerator(ImageSize size, int frame_rate, auto should_stop) -> ImageGenerator {
    const auto [width, height] = size;
    const auto tp = cv::Point(width / 2, height / 2);
    const auto tc = cv::Scalar(0, 0, 255);

    chrono::FrameRateController fr_controller(frame_rate);
    Image yuv(height + height / 2, width, CV_8UC1);
    Image bgr;
    int index{};
    std::string text;

    while (!should_stop()) {
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

        text = std::format("{}", index);
        cv::cvtColor(yuv, bgr, cv::COLOR_YUV420p2BGR);
        cv::putText(bgr, text, tp, cv::FONT_HERSHEY_SIMPLEX, 2.0, tc);

        co_yield bgr;
        index++;
        fr_controller.Sleep();
    }
}

static auto CreateCameraGenerator(cv::VideoCapture& cap, auto should_stop) -> ImageGenerator {
    Image image;
    while (!should_stop()) {
        if (!cap.read(image)) {
            println("End of video");
            break;
        }
        Image bgr;
        cv::cvtColor(image, bgr, cv::COLOR_RGB2BGR);
        co_yield bgr;
    }
}

static std::atomic_bool should_exit{};

auto main(int argc, char* argv[]) -> int {
    if (argc < 2) {
        println("Example Usage:\n {} --url rtmp://127.0.0.1:8080/live --display", argv[0]);
        return 1;
    }

    signal(SIGINT, [](int) {
        println("\nUser Interrupt");
        should_exit.store(true);
    });

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    auto cli = argparse::CLI(argc, argv);
    const std::string window_name{"RTMP Sender"};
    std::string url{};
    bool display_image{};

    cli.VisitIfContains<std::string>("--url", [&url](std::string url_) {
        println("Using user provided url={}", url_);
        url = url_;
    });

    if (cli.Contains("--display")) {
        display_image = true;
    }

    if (display_image) {
        try {
            cv::namedWindow(window_name, cv::WINDOW_AUTOSIZE);
        } catch (cv::Exception& exc) {
            println("Display is not supported by opencv");
            display_image = false;
        }
    }

    cv::VideoCapture cap{0, cv::CAP_V4L2};
    if (cap.isOpened()) {
        println("Found default camera");
    }

    H264Encoder::Settings settings;

    if (!cap.isOpened()) {
        settings.size = ImageSize(1280, 720);
        settings.bitrate = 4000000;
        settings.frame_rate = 30;
    } else {
        settings.size = ImageSize(cap.get(cv::CAP_PROP_FRAME_WIDTH), cap.get(cv::CAP_PROP_FRAME_HEIGHT));
        settings.frame_rate = cap.get(cv::CAP_PROP_FPS);
        settings.bitrate = 4000000;
    }
    println("H264 Encoder settings width={} height={} fps={} bitrate={}", settings.size.width, settings.size.height,
            settings.frame_rate, settings.bitrate);

    H264Encoder encoder{};
    auto result = encoder.Open(settings);
    if (!result) {
        println("Open Encoder failed with error {}", result.error().what());
        return 1;
    }

    av::UniqueFormatContextPtr fmt_ctx([] {
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
    av::UniqueIoContextPtr rtmp_ctx{[&] {
        AVIOContext* raw;
        avio_open2(&raw, url.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
        return raw;
    }()};
    if (!rtmp_ctx) {
        println("Failed to establish connection", url);
        return 1;
    }

    av_opt_set(fmt_ctx.get(), "rtmp_live", "live", 0);
    fmt_ctx->pb = rtmp_ctx.get();

    ret = avformat_write_header(fmt_ctx.get(), nullptr);
    if (ret < 0) {
        println("Could not write header!");
        return 1;
    }

    auto generator_should_stop = [] { return should_exit.load(std::memory_order_relaxed); };
    auto generator = [&] {
        if (cap.isOpened())
            return CreateCameraGenerator(cap, generator_should_stop);
        else
            return CreateRgbFlowEffectGenerator(settings.size, settings.frame_rate, generator_should_stop);
    }();

    for (auto image : generator) {
        const auto result = encoder.Encode(image, [&](av::UniquePacketPtr pkt) {
            av_packet_rescale_ts(pkt.get(), codec_ctx->time_base, video_stream->time_base);
            pkt->stream_index = video_stream->index;
            ret = av_interleaved_write_frame(fmt_ctx.get(), pkt.get());
            if (ret < 0) {
                println("Failed to write packet!");
            }
        });

        if (!result) {
            println("Encode failed. {}", result.error().what());
        }

        if (display_image) {
            cv::imshow("Test", image);
            cv::waitKey(1);
        }
    }

    println("Exiting");
    return 0;
}