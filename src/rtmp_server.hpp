#pragma once

#include <string>
#include <chrono>
#include <thread>
#include <functional>

#include <oryx/expected.hpp>
#include <oryx/spsc_queue.hpp>

#include "av_helpers.hpp"
#include "av_error.hpp"

namespace oryx {

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
    auto Decode(AVPacket* packet) -> void_expected<av::Error>;
    auto OpenCodecContext() -> void_expected<av::Error>;

    void ReadWorker(std::stop_token stoken);
    void DecodeWorker(std::stop_token stoken);

    Settings settings_;
    av::UniqueFramePtr frame_;
    av::UniqueCodecContextPtr dec_ctx_;
    av::UniqueSwsContextPtr sws_ctx_;
    av::UniqueFormatContextPtr fmt_ctx_;
    folly::ProducerConsumerQueue<av::UniquePacketPtr> queue_;
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

}  // namespace oryx