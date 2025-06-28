#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <functional>

#include "expected.hpp"
#include "libav_helpers.hpp"
#include "spsc_queue.h"

namespace st {

/**
 * @brief Single RTMP Connection server. Only accepts one connection at a time
 */
class RtmpServer {
public:
    struct Settings {
        std::string url;
        std::chrono::milliseconds buffer_time;
        size_t queue_size;
    };

    struct StreamInfo {
        std::string codec;
        std::string stream_fmt;
        ImageSize resolution;
        int stream_index;
    };

    using OnImageFn = std::function<void(Image)>;
    using OnErrorFn = std::function<void(Error)>;
    using OnConnectedFn = std::function<void(StreamInfo)>;
    using OnDisconnectedFn = std::function<void()>;

    RtmpServer(Settings settings);
    ~RtmpServer();

    void Start();
    void Stop();

    void SetImageHandler(OnImageFn on_image);
    void SetErrorHandler(OnErrorFn on_error);
    void SetConnectedHandler(OnConnectedFn on_connect);
    void SetDisconnectedHandler(OnDisconnectedFn on_disconnect);

private:
    void SubmitError(Error&& error) const;
    auto Decode(AVPacket* packet) -> void_expected;
    auto OpenCodecContext() -> void_expected;

    void ReadWorker(std::stop_token stoken);
    void DecodeWorker(std::stop_token stoken);

    Settings settings_;
    libav::UniqueFramePtr frame_;
    libav::UniqueCodecContextPtr dec_ctx_;
    libav::UniqueSwsContextPtr sws_ctx_;
    libav::UniqueFormatContextPtr fmt_ctx_;
    folly::ProducerConsumerQueue<libav::UniquePacketPtr> queue_;
    OnImageFn on_image_;
    OnErrorFn on_error_;
    OnConnectedFn on_connect_;
    OnDisconnectedFn on_disconnect_;
    std::unique_ptr<std::jthread> read_worker_;
    std::unique_ptr<std::jthread> decode_worker_;
    int video_stream_index_;

    uint8_t* buffer_data[4];
    int buffer_ls[4];
    int buffer_data_size;
};

}  // namespace st