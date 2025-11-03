#pragma once

#include "Rect.hpp"
#include <vector>

namespace fort {
namespace artemis {
typedef std::vector<Rect> Partition;
void PartitionRectangle(const Rect &rect, size_t partitions, Partition &result);

void AddMargin(const Size &maxSize, int margin, Partition &result);

} // namespace artemis
} // namespace fort
