#pragma once

#include <opencv2/core/core.hpp>

#include <vector>

typedef std::vector<cv::Rect> Partition;


void PartitionRectangle(const cv::Rect & rect, size_t partitions, Partition & result);

void PartitionImage(const cv::Mat & img, size_t partitions, Partition & result);

void AddMargin(const cv::Size & maxSize, size_t margin, Partition & result);
