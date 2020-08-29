#pragma once

#include <opencv2/core.hpp>

namespace fort {
namespace artemis {

cv::Rect GetROICenteredAt(const cv::Point & center,
                          const cv::Size & roiSize,
                          const cv::Size & bound);

} // namespace artemis
} // namespace fort
