#pragma once

#include <Eigen/Core>
#include <Eigen/src/Core/Matrix.h>

namespace fort {
namespace artemis {

class Size : public Eigen::Vector2i {
public:
	Size();
	Size(int width, int height);
	Size(const Eigen::Vector2i &other);

	inline int width() const {
		return x();
	}

	inline int height() const {
		return y();
	}
};

class Rect : public Eigen::Vector4i {
public:
	Rect();
	Rect(const Eigen::Vector2i &topLeft, const Eigen::Vector2i &size);

	Eigen::Vector2i TopLeft() const {
		return block<2, 1>(0, 0);
	}

	artemis::Size Size() const {
		return {block<2, 1>(2, 0)};
	}

	int width() const {
		return z();
	}

	int &width() {
		return z();
	}

	int height() const {
		return w();
	}

	int &height() {
		return w();
	}
};

Rect GetROICenteredAt(
    const Eigen::Vector2i &center, const Size &roiSize, const Size &bound
);

} // namespace artemis
} // namespace fort
