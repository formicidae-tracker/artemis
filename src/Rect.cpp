#include "Rect.hpp"

namespace fort {
namespace artemis {

Rect GetROICenteredAt(
    const Eigen::Vector2i &center, const Size &roiSize, const Size &bound
) {
	int x = std::clamp(
	    center.x() - roiSize.width() / 2,
	    0,
	    bound.width() - roiSize.width()
	);

	int y = std::clamp(
	    center.y() - roiSize.height() / 2,
	    0,
	    bound.height() - roiSize.height()
	);

	return {{x, y}, roiSize};
}

Size::Size() {}

Size::Size(int width, int height)
    : Eigen::Vector2i{width, height} {}

Size::Size(const Eigen::Vector2i &other)
    : Eigen::Vector2i{other} {}

Rect::Rect(){};

Rect::Rect(const Eigen::Vector2i &topLeft, const Eigen::Vector2i &size)
    : Eigen::Vector4i{topLeft.x(), topLeft.y(), size.x(), size.y()} {}

} // namespace artemis
} // namespace fort
