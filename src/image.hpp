#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>

namespace oryx {

struct ImageSize {
    int width;
    int height;

    auto operator==(const ImageSize& size) const -> bool = default;
};

using ByteVector = std::vector<uint8_t>;
using Image = cv::Mat;

}  // namespace oryx