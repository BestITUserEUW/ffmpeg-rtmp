#include <print>

#include <opencv2/opencv.hpp>

extern "C" {
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libavutil/timestamp.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "ffmpeg_decoder.hpp"

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
    constexpr auto url = "rtmp://127.0.0.1:8080/live";
    int counter{};
    AVDictionary* options = NULL;
    AVFormatContext* fmt_ctx = nullptr;
    AVIOContext* io_ctx = nullptr;
    AVStream* video_stream = nullptr;

    int ret;
    if ((ret = av_dict_set(&options, "listen", "1", 0)) < 0) {
        println("Failed to set listen mode for server");
        return ret;
    }

    ret = avformat_open_input(&fmt_ctx, url, nullptr, &options);
    if (ret < 0) {
        println("Failed to open server");
        return 1;
    }
    println("Connection opened");

    if (avformat_find_stream_info(fmt_ctx, NULL) < 0) {
        println("Could not find stream information");
        return 1;
    }

    AVPacket* pkt = av_packet_alloc();
    if (!pkt) {
        println("Could not allocate packet");
        return 1;
    }

    while (av_read_frame(fmt_ctx, pkt) >= 0) {
        printf("video_frame n:%d\n", counter++);
        av_packet_unref(pkt);
        if (ret < 0) break;
    }

    println("Exiting");
    return 0;
}