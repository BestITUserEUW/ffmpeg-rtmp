#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>

namespace st {

struct ImageSize {
    int width;
    int height;

    bool operator==(const ImageSize& size) const = default;
};

using ByteVector = std::vector<uint8_t>;
using Image = cv::Mat;

}  // namespace st