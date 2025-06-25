#pragma once

#include <vector>

#include <opencv2/core/mat.hpp>

namespace st {

using H264Image = std::vector<uint8_t>;
using Image = cv::Mat;

}  // namespace st