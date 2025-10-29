#pragma once

#include "Rect.hpp"
#include <opencv2/opencv.hpp>
#include <vector>

namespace fort {
namespace artemis {
typedef std::vector<Rect> Partition;
void PartitionRectangle(const Rect &rect, size_t partitions, Partition &result);

void PartitionImage(const cv::Mat &img, size_t partitions, Partition &result);

void AddMargin(const Size &maxSize, size_t margin, Partition &result);

} // namespace artemis
} // namespace fort
