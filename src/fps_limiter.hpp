#pragma once

#include <chrono>
#include <thread>

namespace st {

class FpsLimiter {
public:
    explicit FpsLimiter(int target_fps)
        : target_frame_duration_(1000 / target_fps),
          last_frame_time_(std::chrono::steady_clock::now()) {}

    void Sleep() {
        using namespace std::chrono;

        auto now = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(now - last_frame_time_);

        if (elapsed.count() < target_frame_duration_) {
            std::this_thread::sleep_for(milliseconds(target_frame_duration_ - elapsed.count()));
        }

        last_frame_time_ = steady_clock::now();  // Reset after sleep to maintain rhythm
    }

private:
    int target_frame_duration_;  // in milliseconds
    std::chrono::steady_clock::time_point last_frame_time_;
};

}  // namespace st