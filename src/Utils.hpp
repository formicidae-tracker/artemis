#pragma once

#include <opencv2/core.hpp>

namespace fort {
namespace artemis {

cv::Rect GetROICenteredAt(const cv::Point & center,
                          const cv::Size & roiSize,
                          const cv::Size & bound);

cv::Mat GetROICenteredAtBlackBorder(const cv::Mat & image,
                                    const cv::Point & position,
                                    const cv::Size & size);


} // namespace artemis
} // namespace fort
