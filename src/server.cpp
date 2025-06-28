#include <print>

#include <csignal>

#include <opencv2/opencv.hpp>
#include <opencv2/core/utils/logger.hpp>

#include "enchantum.hpp"
#include "rtmp_server.hpp"
#include "argparse.hpp"

using std::println;
using namespace st;

std::unique_ptr<RtmpServer> server;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        println("Example Usage:\n {} rtmp://127.0.0.1:8080/live", argv[0]);
        return 1;
    }

    signal(SIGINT, [](int) {
        println("\nUser Interrupt");
        server.reset();
        exit(0);
    });

    cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_ERROR);

    auto cli = argparse::CLI(argc, argv);
    const std::string window_name{"RTMP Server"};
    RtmpServer::Settings settings;
    settings.buffer_time = std::chrono::milliseconds(1000);
    settings.queue_size = 64;
    bool display_image{};

    cli.VisitIfContains<std::string>("--url", [&settings](std::string url_) {
        println("Using user provided url={}", url_);
        settings.url = url_;
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

    int counter{};
    server = std::make_unique<RtmpServer>(settings);
    server->SetConnectedHandler([](RtmpServer::StreamInfo info) {
        println("Client connected codec={} fmt={} width={} height={} stream_index={}", info.codec, info.stream_fmt,
                info.resolution.width, info.resolution.height, info.stream_index);
    });
    server->SetDisconnectedHandler([] { println("Client disconnected"); });
    server->SetErrorHandler([](Error error) { println("Server error={}", error.what()); });
    server->SetImageHandler([&](Image image) {
        if (display_image) {
            cv::imshow(window_name, image);
            cv::waitKey(1);
        } else {
            println("Decoded image={}", counter++);
        }
    });
    server->Start();

    while (1) {
        usleep(10000);
    }

    return 0;
}