#include "Partitions.hpp"

namespace fort {
namespace artemis {

void PartitionInTwo(const Rect &rect, size_t partitions, Rect &a, Rect &b) {
	Size            sizeA, sizeB;
	Eigen::Vector2i tlB;
	if (rect.height() > rect.width()) {
		sizeA = {rect.width(), rect.height() / int(partitions)};
		sizeB = {rect.width(), rect.height() - sizeA.height()};
		tlB   = {rect.x(), rect.y() + sizeA.height()};
	} else {
		sizeA = {rect.width() / int(partitions), rect.height()};
		sizeB = {rect.width() - sizeA.width(), rect.height()};
		tlB   = {rect.x() + sizeA.width(), rect.y()};
	}
	a = {rect.TopLeft(), sizeA};
	b = {tlB, sizeB};
}

void PartitionRectangle(
    const Rect &rect, size_t partitions, Partition &results
) {
	if (partitions == 1) {
		results.push_back(rect);
		return;
	}

	if (partitions % 2 == 1) {
		Rect single, rest;
		PartitionInTwo(rect, partitions, single, rest);
		results.push_back(single);
		PartitionRectangle(rest, partitions - 1, results);
		return;
	}

	Rect a, b;
	PartitionInTwo(rect, 2, a, b);
	PartitionRectangle(a, partitions / 2, results);
	PartitionRectangle(b, partitions / 2, results);
}

void AddMargin(const Size &size, int margin, Partition &partitions) {
	for (auto &p : partitions) {
		if (p.x() >= margin) {
			p.x() -= margin;
			p.width() += int(margin);
		} else {
			p.width() += p.x();
			p.x() = 0;
		}

		if (p.x() + p.width() + margin <= size.width()) {
			p.width() += margin;
		} else {
			p.width() = size.width() - p.x();
		}
		if (p.y() >= margin) {
			p.y() -= margin;
			p.height() += margin;
		} else {
			p.height() += p.y();
			p.y() = 0;
		}
		if (p.y() + p.height() + margin <= size.height()) {
			p.height() += margin;
		} else {
			p.height() = size.height() - p.y();
		}
	}
}

} // namespace artemis
} // namespace fort
